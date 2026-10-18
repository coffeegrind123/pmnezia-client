#import "TrustTunnelClientFactory.h"

@implementation TrustTunnelClientFactory

+ (nullable VpnClient *)clientWithConfig:(NSString *)config
                   connectionInfoHandler:(ConnectionInfoHandler)connectionInfoHandler
                      stateChangeHandler:(StateChangeHandler)stateChangeHandler
{
    return [[VpnClient alloc] initWithConfig:config
                       connectionInfoHandler:connectionInfoHandler
                          stateChangeHandler:stateChangeHandler];
}

@end
