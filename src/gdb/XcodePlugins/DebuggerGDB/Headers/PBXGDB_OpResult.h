/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "NSObject.h"

@class NSString, PBXGDB_CType;

@interface PBXGDB_OpResult : NSObject
{
    id <NSObject> _object;
    PBXGDB_CType *_type;
    NSString *_name;
}

+ (id)resultWithObject:(id)arg1 type:(id)arg2;
- (id)name;
- (void)setName:(id)arg1;
- (id)resultType;
- (void)setResultType:(id)arg1;
- (id)resultObject;
- (void)dealloc;
- (id)initWithObject:(id)arg1 type:(id)arg2;

@end

