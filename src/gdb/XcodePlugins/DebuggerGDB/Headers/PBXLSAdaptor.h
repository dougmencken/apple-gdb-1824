/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import <DebuggerGDB/PBXLSPlugableClass.h>

@class NSMutableDictionary, PBXLSLaunchSession;

@interface PBXLSAdaptor : PBXLSPlugableClass
{
    PBXLSLaunchSession *_launchSession;
    BOOL _taskIsRunning;
    NSMutableDictionary *_clientInfo;
}

+ (id)name;
- (id)clientInfoDictionary;
- (void)logString:(id)arg1;
- (void)dumpLog;
- (BOOL)taskIsRunning;
- (void)setTaskIsRunning:(BOOL)arg1;
- (void)executableDidFinish;
- (void)executableDidStart;
- (id)parseAndApproveOutputFromDebugger:(id)arg1;
- (id)parseAndApproveInputFromConsole:(id)arg1;
- (void)unconfigureUI;
- (void)configureUI;
- (void)configureAdaptor;
- (id)PBXLaunchSession;
- (id)executableLauncher;
- (void)updateEnvironmentForLaunch:(id)arg1;
- (void)updateArgumentsForLaunch:(id)arg1;
- (id)name;
- (id)initWithSession:(id)arg1;

@end
