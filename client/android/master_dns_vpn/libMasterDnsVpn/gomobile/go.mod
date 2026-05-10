// Module wraps upstream MasterDnsVPN as a gomobile-compatible package.
// Built into libmasterdnsvpn.aar by `gomobile bind` — see ../README.md.
module org.amnezia.vpn/protocol/masterdnsvpn/libmasterdnsvpn

go 1.22

// The MasterDnsVPN core lives in masterdnsvpn-go/internal/client; it is pulled
// in as a Go module dependency at the version pinned by the conan recipe
// `amnezia-mdnsvpn-android/<ver>`. Bumping the upstream version is a
// recipe-side change followed by `go mod tidy` here.
require github.com/masterking32/MasterDnsVPN v0.0.0-00010101000000-000000000000
