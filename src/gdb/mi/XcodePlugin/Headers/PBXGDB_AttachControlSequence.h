/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_BaseThreadSequence.h"

@interface PBXGDB_AttachControlSequence : PBXGDB_BaseThreadSequence
{
    int _processID;
}

- (void)execute;
- (void)_gotAttachVerify:(id)arg1;
- (void)_doAttach;
- (BOOL)handleErrorForMICommand:(id)arg1;
- (void)_didFinishWithError;
- (void)_noAttachCommand;
- (void)_resultAllThreadsRunning:(id)arg1;
- (void)_didAttach:(id)arg1;
- (void)_didSendAttachCommand;
- (void)didFinish;
- (int)processID;
- (void)setProcessID:(int)arg1;

@end

