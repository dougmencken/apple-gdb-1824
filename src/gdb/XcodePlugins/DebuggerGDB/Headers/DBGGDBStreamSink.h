/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "NSObject.h"

#import "DBGGDBByteStreamConsuming-Protocol.h"
#import "DBGGDBCharacterStreamConsuming-Protocol.h"
#import "DBGGDBLineStreamConsuming-Protocol.h"
#import "DBGGDBStreamConsumerRetaining-Protocol.h"
#import "DBGGDBStreamConsuming-Protocol.h"

@interface DBGGDBStreamSink : NSObject <DBGGDBStreamConsuming, DBGGDBByteStreamConsuming, DBGGDBCharacterStreamConsuming, DBGGDBLineStreamConsuming, DBGGDBStreamConsumerRetaining>
{
}

+ (id)streamSink;
- (void)stream:(id)arg1 processLine:(id)arg2;
- (void)stream:(id)arg1 processCharacters:(id)arg2;
- (void)stream:(id)arg1 processBytes:(id)arg2;
- (void)streamDidEnd:(id)arg1;

@end

