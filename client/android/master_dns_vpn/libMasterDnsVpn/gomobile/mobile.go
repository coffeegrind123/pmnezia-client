// Package libmasterdnsvpn is the gomobile binding surface that wraps the
// upstream MasterDnsVPN client core into a JVM-callable AAR.
//
// Built into libmasterdnsvpn.aar via:
//
//	gomobile bind -target=android/arm64,arm,amd64,386 -androidapi 21 .
//
// The AAR exposes class org.amnezia.vpn.protocol.masterdnsvpn.libMasterDnsVpn.LibMasterDnsVpn
// (matching the gomobile naming convention: package-name -> Java class name).
//
// The client-side Kotlin (../../src/main/kotlin/MasterDnsVpn.kt) calls
// LibMasterDnsVpn.startClient(configPath) / .stopClient() / .isRunning().
//
// We deliberately keep the surface tiny — three functions — because the
// MasterDnsVPN core is configured entirely from the TOML file the Kotlin side
// writes. Adding more knobs here would create an Amnezia-specific schema that
// drifts from the upstream-format TOML the operator gives the user.
package libmasterdnsvpn

import (
	"errors"
	"fmt"
	"sync"

	"github.com/masterking32/MasterDnsVPN/internal/client"
	"github.com/masterking32/MasterDnsVPN/internal/config"
)

// One core per process — gomobile callers can't usefully run multiple
// independent VPN tunnels in the same JVM, and the upstream client uses
// package-level state for its resolver pool / ARQ queues.
var (
	mu      sync.Mutex
	running *client.Client
)

// StartClient boots the bundled MasterDnsVPN client core from a TOML config
// file at configPath. Returns "" on success, or a non-empty error string the
// JVM caller should surface as a VpnStartException.
//
// The function is safe to call when a previous core is already running — it
// returns an error rather than silently double-starting.
func StartClient(configPath string) string {
	mu.Lock()
	defer mu.Unlock()

	if running != nil {
		return "MasterDnsVPN core already running"
	}

	cfg, err := config.LoadClientConfig(configPath)
	if err != nil {
		return fmt.Sprintf("load client config: %v", err)
	}

	c, err := client.New(cfg)
	if err != nil {
		return fmt.Sprintf("instantiate client: %v", err)
	}

	if err := c.Start(); err != nil {
		return fmt.Sprintf("start client: %v", err)
	}

	running = c
	return ""
}

// StopClient gracefully tears down the running core. Returns "" on success
// (including the no-running-core case) or a non-empty error string.
func StopClient() string {
	mu.Lock()
	defer mu.Unlock()

	if running == nil {
		return ""
	}

	err := running.Stop()
	running = nil

	if err != nil {
		// errors.Is(err, context.Canceled) is the expected shutdown signal — treat
		// as success so the JVM doesn't log a spurious warning on a clean stop.
		if errors.Is(err, ErrClosedNormally) {
			return ""
		}
		return fmt.Sprintf("stop client: %v", err)
	}
	return ""
}

// IsRunning reports whether a core is currently serving traffic. Mainly used
// by Kotlin diagnostics; the SOCKS5 listener probe in MasterDnsVpn.kt is the
// authoritative readiness signal.
func IsRunning() bool {
	mu.Lock()
	defer mu.Unlock()
	return running != nil
}

// ErrClosedNormally is the sentinel the upstream client returns from Stop()
// after a clean shutdown — kept here so we can swallow it in StopClient
// without leaking implementation details to the JVM caller.
var ErrClosedNormally = errors.New("masterdnsvpn: closed normally")
