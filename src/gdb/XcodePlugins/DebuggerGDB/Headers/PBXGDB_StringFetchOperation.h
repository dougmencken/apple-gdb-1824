/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import <DebuggerGDB/PBXGDB_EvalOperation.h>

@interface PBXGDB_StringFetchOperation : PBXGDB_EvalOperation
{
}

- (Class)fetchResultSequenceClass;
- (Class)stringFetchSequenceClass;
- (unsigned long long)defaultEncoding;
- (unsigned long long)charactersToRead;
- (unsigned long long)bytesPerCharacter;
- (void)perform;
- (void)sequenceDidFinish:(id)arg1;

@end

