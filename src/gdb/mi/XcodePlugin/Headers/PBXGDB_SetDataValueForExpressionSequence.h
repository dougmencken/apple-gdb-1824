/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_BaseDataValueSequence.h"

@class NSString, PBXGDB_VarObjDataValue2;

@interface PBXGDB_SetDataValueForExpressionSequence : PBXGDB_BaseDataValueSequence
{
    PBXGDB_VarObjDataValue2 *_targetDV;
    BOOL _bindToFrame;
    NSString *_blockStartAddress;
}

- (void)execute;
- (void)_gotResultCreatedVarObj:(id)arg1;
- (void)_gotResultVarObjValue:(id)arg1;
- (id)targetDV;
- (void)setTargetDV:(id)arg1;
- (void)didFinish;
- (id)expressionString;
- (void)setExpressionString:(id)arg1 bindToFrame:(BOOL)arg2;
- (void)setExpressionString:(id)arg1 withBlockStartAddress:(id)arg2;
- (BOOL)handleErrorForMICommand:(id)arg1;
- (void)dealloc;

@end

