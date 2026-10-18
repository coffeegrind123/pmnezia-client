#ifndef TRUSTTUNNEL_CLIENT_FACTORY_H
#define TRUSTTUNNEL_CLIENT_FACTORY_H

#import <Foundation/Foundation.h>
#import <VpnClientFramework/VpnClient.h>

NS_ASSUME_NONNULL_BEGIN

// VpnClient.h declares -initWithConfig:... non-null under
// NS_ASSUME_NONNULL, but it returns nil when the TOML does not parse or the
// network monitor fails to start. Swift would treat that nil as a valid
// object; this factory gives Swift an honest optional.
@interface TrustTunnelClientFactory : NSObject

+ (nullable VpnClient *)clientWithConfig:(NSString *)config
                   connectionInfoHandler:(ConnectionInfoHandler)connectionInfoHandler
                      stateChangeHandler:(StateChangeHandler)stateChangeHandler
    NS_SWIFT_NAME(makeClient(config:connectionInfoHandler:stateChangeHandler:));

@end

NS_ASSUME_NONNULL_END

#endif // TRUSTTUNNEL_CLIENT_FACTORY_H
