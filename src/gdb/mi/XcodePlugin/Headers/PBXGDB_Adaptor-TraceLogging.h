/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_Adaptor.h"

@interface PBXGDB_Adaptor (TraceLogging)
+ (void)dumpLogs;
- (void)logMessage:(id)arg1;
- (void)logString:(id)arg1;
- (void)logCommentString:(id)arg1 withTimeStamp:(BOOL)arg2;
- (void)logStringToGDB:(id)arg1;
- (void)logStringFromGDB:(id)arg1;
- (void)dumpLog;
- (void)_logWithPrefix:(id)arg1 string:(id)arg2 suffix:(id)arg3;
- (BOOL)loggingEnabled;
- (void)_initializeLogging;
- (id)_logFileName;
@end

