/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_MICommand.h"

@class NSArray, NSString, PBXGDB_MIResultList;

@interface PBXGDB_MILiteStackFramesCommand : PBXGDB_MICommand
{
    long long _depth;
    long long _valid;
    NSArray *_framesInfoItems;
    NSString *_currentFPString;
    NSString *_currentPCString;
    PBXGDB_MIResultList *_framesList;
}

- (unsigned long long)currentIPAddress;
- (id)currentIPString;
- (unsigned long long)currentFPAddress;
- (id)currentFPString;
- (void)setCurrentFPString:(id)arg1;
- (void)setCurrentPCString:(id)arg1;
- (void)setupCurrentFrameInfoForIndex:(unsigned long long)arg1;
- (unsigned long long)numberOfFrameInfos;
- (unsigned long long)stackDepth;
- (BOOL)infoIsValid;
- (void)didFinishWithResultList:(id)arg1;
- (void)dealloc;
- (void)setFrameInfoLimit:(long long)arg1;

@end

