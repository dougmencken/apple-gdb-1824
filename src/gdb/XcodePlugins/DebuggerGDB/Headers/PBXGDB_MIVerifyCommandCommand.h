/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import <DebuggerGDB/PBXGDB_MICommand.h>

@interface PBXGDB_MIVerifyCommandCommand : PBXGDB_MICommand
{
    BOOL _defined;
    BOOL _implemented;
}

- (BOOL)commandIsImplemented;
- (BOOL)commandIsDefined;
- (void)didFinishWithResultList:(id)arg1;
- (void)verifyCommand:(id)arg1;

@end

