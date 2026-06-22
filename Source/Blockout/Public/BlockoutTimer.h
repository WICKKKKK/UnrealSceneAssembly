// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BlockoutLog.h"

class BLOCKOUT_API FBlockoutTimer
{
public:
	FBlockoutTimer(const FString& InTag, const FString& InLogData)
	{
		StartTime = FDateTime::Now();
		LogData = InTag + TEXT("::") + InLogData;
	}

	~FBlockoutTimer()
	{
		UE_LOG(LogBlockout, Warning, TEXT("%s time cost %f"), *LogData, FTimespan(FDateTime::Now().GetTicks() - StartTime.GetTicks()).GetTotalMilliseconds());
	}

private:
	FDateTime StartTime;
	FString LogData;
};

#define FA_BLOCKOUT_TIMER(Tag, LogData) FBlockoutTimer Tag##BlockoutTimer(Tag, TEXT(LogData))
