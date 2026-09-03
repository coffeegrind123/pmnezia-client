package org.amnezia.vpn

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.graphics.ImageFormat
import android.os.Bundle
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_UP
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts.RequestPermission
import androidx.camera.core.Camera
import androidx.camera.core.CameraSelector
import androidx.camera.core.FocusMeteringAction
import androidx.camera.core.FocusMeteringAction.FLAG_AE
import androidx.camera.core.FocusMeteringAction.FLAG_AF
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.min
import org.amnezia.vpn.databinding.CameraPreviewBinding
import org.amnezia.vpn.qt.QtAndroidController
import org.amnezia.vpn.util.Log
import zxingcpp.BarcodeReader

private const val TAG = "CameraActivity"

private val SUPPORTED_IMAGE_FORMATS = intArrayOf(
    ImageFormat.YUV_420_888,
    ImageFormat.YUV_422_888,
    ImageFormat.YUV_444_888
)

class CameraActivity : ComponentActivity() {

    private lateinit var viewBinding: CameraPreviewBinding
    private lateinit var cameraProvider: ProcessCameraProvider

    // zxing-cpp's read() is synchronous and CPU bound, so it must not run on the main thread
    // the way ML Kit's task-based process() could.
    private val analysisExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private val scanFinished = AtomicBoolean(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        viewBinding = CameraPreviewBinding.inflate(layoutInflater)
        setContentView(viewBinding.root)

        checkPermissions(onSuccess = ::startCamera, onFail = ::finish)
    }

    override fun onDestroy() {
        analysisExecutor.shutdown()
        super.onDestroy()
    }

    private fun checkPermissions(onSuccess: () -> Unit, onFail: () -> Unit) {
        if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            onSuccess()
        } else {
            val requestPermissionLauncher =
                registerForActivityResult(RequestPermission()) { isGranted ->
                    if (isGranted) {
                        Toast.makeText(this, "Camera permission granted", Toast.LENGTH_SHORT).show()
                        onSuccess()
                    } else {
                        Toast.makeText(this, "Camera permission denied", Toast.LENGTH_SHORT).show()
                        onFail()
                    }
                }
            requestPermissionLauncher.launch(Manifest.permission.CAMERA)
        }
    }

    private fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)

        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            bindPreview()
            bindImageAnalysis()
        }, ContextCompat.getMainExecutor(this))
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun bindPreview() {
        val viewFinder = viewBinding.viewFinder
        val preview = Preview.Builder().build().also {
            it.setSurfaceProvider(viewFinder.surfaceProvider)
        }

        val camera = cameraProvider.bindToLifecycle(this, CameraSelector.DEFAULT_BACK_CAMERA, preview)

        viewFinder.setOnTouchListener { _, motionEvent ->
            when (motionEvent.action) {
                ACTION_DOWN -> true
                ACTION_UP -> {
                    val point = viewFinder
                        .meteringPointFactory.createPoint(motionEvent.x, motionEvent.x)

                    val action = FocusMeteringAction
                        .Builder(point, FLAG_AF or FLAG_AE).build()

                    camera.cameraControl.startFocusAndMetering(action)
                    true
                }

                else -> false
            }
        }
    }

    private fun bindImageAnalysis() {
        val imageAnalysis = ImageAnalysis.Builder()
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .build()

        val camera = cameraProvider.bindToLifecycle(this, CameraSelector.DEFAULT_BACK_CAMERA, imageAnalysis)

        val barcodeReader = BarcodeReader(
            BarcodeReader.Options(
                formats = setOf(BarcodeReader.Format.QR_CODE),
                tryHarder = true,
                tryRotate = true,
                tryInvert = true,
                tryDownscale = true,
                // Symbols that were located but could not be decoded come back with a non-null
                // error and a valid position. Those drive auto-zoom - see AutoZoom below.
                returnErrors = true
            )
        )

        // zoomState is LiveData and can still be null right after binding, so observe it on the
        // main thread and hand the value to the analysis thread rather than sampling it once.
        val autoZoom = AutoZoom(camera)
        camera.cameraInfo.zoomState.observe(this) { state -> autoZoom.updateMaxZoom(state.maxZoomRatio) }

        // optimization
        val checkedBarcodes = hashSetOf<String>()

        imageAnalysis.setAnalyzer(analysisExecutor) { imageProxy ->
            imageProxy.use { proxy -> analyze(proxy, barcodeReader, autoZoom, checkedBarcodes) }
        }
    }

    private fun analyze(
        imageProxy: ImageProxy,
        barcodeReader: BarcodeReader,
        autoZoom: AutoZoom,
        checkedBarcodes: MutableSet<String>
    ) {
        if (scanFinished.get()) return

        if (imageProxy.format !in SUPPORTED_IMAGE_FORMATS) {
            Log.e(TAG, "Unsupported image format: ${imageProxy.format}")
            return
        }

        val results = try {
            barcodeReader.read(imageProxy)
        } catch (e: Exception) {
            Log.e(TAG, "Processing QR code image failed: ${e.message}")
            return
        }

        val decoded = results.firstOrNull { it.error == null && !it.text.isNullOrEmpty() }
        if (decoded == null) {
            // Nothing readable. If a symbol was located but not decoded it is probably too small
            // in frame, which is the case auto-zoom exists for.
            autoZoom.onFrame(results.firstOrNull(), imageProxy)
            return
        }
        autoZoom.onDecoded()

        val code = decoded.text ?: return
        if (!checkedBarcodes.add(code)) return

        runOnUiThread {
            if (scanFinished.get()) return@runOnUiThread
            if (QtAndroidController.decodeQrCode(code)) {
                if (scanFinished.compareAndSet(false, true)) {
                    stopCamera()
                }
            }
        }
    }

    private fun stopCamera() {
        cameraProvider.unbindAll()
        finish()
    }

    /**
     * Replacement for ML Kit's ZoomSuggestionOptions, which is not available outside Google Play
     * Services. zxing-cpp reports the corner points of every symbol it locates, including ones it
     * failed to decode, so the same "the code is too far away, zoom in" behaviour can be driven
     * from the located symbol's size relative to the frame.
     *
     * All methods are called from the single analysis thread; CameraControl.setZoomRatio is
     * thread-safe.
     */
    private class AutoZoom(private val camera: Camera) {

        // Written from the main thread by the zoomState observer, read from the analysis thread.
        @Volatile
        private var maxZoomRatio = 1f

        private var currentZoom = 1f
        private var smallSymbolFrames = 0
        private var emptyFrames = 0
        private var cooldownFrames = 0

        fun updateMaxZoom(value: Float) {
            if (value.isFinite() && value >= 1f) {
                maxZoomRatio = value
            }
        }

        fun onDecoded() {
            // A successful read means the current zoom is good enough; leave it alone.
            smallSymbolFrames = 0
            emptyFrames = 0
        }

        fun onFrame(located: BarcodeReader.Result?, image: ImageProxy) {
            if (cooldownFrames > 0) {
                cooldownFrames--
                return
            }

            if (located == null) {
                smallSymbolFrames = 0
                if (currentZoom > 1f && ++emptyFrames >= RESET_AFTER_EMPTY_FRAMES) {
                    emptyFrames = 0
                    applyZoom(1f)
                }
                return
            }

            emptyFrames = 0

            val frameExtent = min(image.cropRect.width(), image.cropRect.height()).toFloat()
            if (frameExtent <= 0f) return

            val ratio = symbolExtent(located) / frameExtent
            if (ratio <= 0f || ratio >= TRIGGER_RATIO) {
                smallSymbolFrames = 0
                return
            }

            if (++smallSymbolFrames < TRIGGER_FRAMES) return
            smallSymbolFrames = 0

            // Cap a single step so a wildly small reading cannot slam the lens to max zoom.
            val step = (TARGET_RATIO / ratio).coerceAtMost(MAX_STEP)
            applyZoom(currentZoom * step)
        }

        private fun applyZoom(requested: Float) {
            val target = requested.coerceIn(1f, maxZoomRatio)
            if (abs(target - currentZoom) < currentZoom * MIN_RELATIVE_CHANGE) return

            currentZoom = target
            cooldownFrames = COOLDOWN_FRAMES
            try {
                camera.cameraControl.setZoomRatio(target)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to set zoom ratio $target: ${e.message}")
            }
        }

        /** Longest side of the located symbol, in image pixels. */
        private fun symbolExtent(result: BarcodeReader.Result): Float {
            val p = result.position
            val sides = floatArrayOf(
                dist(p.topLeft.x, p.topLeft.y, p.topRight.x, p.topRight.y),
                dist(p.topRight.x, p.topRight.y, p.bottomRight.x, p.bottomRight.y),
                dist(p.bottomRight.x, p.bottomRight.y, p.bottomLeft.x, p.bottomLeft.y),
                dist(p.bottomLeft.x, p.bottomLeft.y, p.topLeft.x, p.topLeft.y)
            )
            return sides.max()
        }

        private fun dist(x1: Int, y1: Int, x2: Int, y2: Int): Float =
            hypot((x2 - x1).toFloat(), (y2 - y1).toFloat())

        private companion object {
            // Zoom in once a located symbol spans less than this fraction of the shorter frame side.
            const val TRIGGER_RATIO = 0.18f

            // Aim to bring it up to roughly this fraction.
            const val TARGET_RATIO = 0.35f

            const val MAX_STEP = 2.0f
            const val MIN_RELATIVE_CHANGE = 0.1f

            // Consecutive too-small frames before acting, so a single bad reading does not zoom.
            const val TRIGGER_FRAMES = 3

            // Frames to ignore while the lens settles after a zoom change.
            const val COOLDOWN_FRAMES = 8

            // Zoom back out after this many frames with nothing in view at all.
            const val RESET_AFTER_EMPTY_FRAMES = 40
        }
    }
}
