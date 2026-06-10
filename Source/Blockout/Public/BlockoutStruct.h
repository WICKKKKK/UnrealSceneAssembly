#pragma once

#include "CoreMinimal.h"
#include "BlockoutEnum.h"

#include "BlockoutStruct.generated.h"


USTRUCT(BlueprintType)
struct FBlockoutFloat
{
	GENERATED_BODY()

	//Toggle Text Preview
	UPROPERTY(EditAnywhere)
	bool bShowTextLabel = false;

	//Parameter Text DisplayName
	UPROPERTY(EditAnywhere, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FString TextLabel = FString("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.0f;

	FBlockoutFloat(){}

	FBlockoutFloat(float InFloat, bool InShowTextLabel=false, FString InTextLabel=FString(""))
		: bShowTextLabel(InShowTextLabel)
		, TextLabel(InTextLabel)
		, Value(InFloat)
	{
	}

	FBlockoutFloat& operator=(const float& RHS)
	{
		Value = RHS;
		return *this;
	}

	FString ToString() const
	{
		if (bShowTextLabel)
			return FString::Printf(TEXT("%s: %.3f\n"), *TextLabel, Value);
		return FString("");
	}
};


USTRUCT(BlueprintType)
struct FBlockoutInt
{
	GENERATED_USTRUCT_BODY()

	//Toggle Text Preview
	UPROPERTY(EditAnywhere)
	bool bShowTextLabel = false;

	//Parameter Text DisplayName
	UPROPERTY(EditAnywhere, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FString TextLabel = FString("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Value = 0;

	FBlockoutInt(){}

	FBlockoutInt(int InInt, bool InShowTextLabel=false, FString InTextLabel=FString(""))
		: bShowTextLabel(InShowTextLabel)
		, TextLabel(InTextLabel)
		, Value(InInt)
	{
	}

	FBlockoutInt& operator=(const int& RHS)
	{
		Value = RHS;
		return *this;
	}

	FString ToString() const
	{
		if (bShowTextLabel)
			return FString::Printf(TEXT("%s: %d\n"), *TextLabel, Value);
		return FString("");
	}
};


USTRUCT(BlueprintType)
struct FBlockoutBool
{
	GENERATED_USTRUCT_BODY()

	//Toggle Text Preview
	UPROPERTY(EditAnywhere)
	bool bShowTextLabel = false;

	//Parameter Text DisplayName
	UPROPERTY(EditAnywhere, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FString TextLabel = FString("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Value = false;

	FBlockoutBool(){}
	
	FBlockoutBool(bool InBool, bool InShowTextLabel=false, FString InTextLabel=FString(""))
		: bShowTextLabel(InShowTextLabel)
		, TextLabel(InTextLabel)
		, Value(InBool)
	{
	}

	FBlockoutBool& operator=(const bool& RHS)
	{
		Value = RHS;
		return *this;
	}

	FString ToString() const
	{
		if (bShowTextLabel)
			return FString::Printf(TEXT("%s: %s\n"), *TextLabel, Value ? TEXT("True") : TEXT("False"));
		return FString("");
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFString
{
	GENERATED_USTRUCT_BODY()

	//Toggle Text Preview
	UPROPERTY(EditAnywhere)
	bool bShowTextLabel = false;

	//Parameter Text DisplayName
	UPROPERTY(EditAnywhere, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FString TextLabel = FString("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Value = FString("");

	FBlockoutFString(){}

	FBlockoutFString(FString InString, bool InShowTextLabel=false, FString InTextLabel=FString(""))
		: bShowTextLabel(InShowTextLabel)
		, TextLabel(InTextLabel)
		, Value(InString)
	{
	}

	FBlockoutFString& operator=(const FString& RHS)
	{
		Value = RHS;
		return *this;
	}

	FString ToString() const
	{
		if (bShowTextLabel)
			return FString::Printf(TEXT("%s: %s\n"), *TextLabel, *Value);
		return FString("");
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFVector
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat X = FBlockoutFloat(0.0f, false, "X");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat Y = FBlockoutFloat(0.0f, false, "Y");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat Z = FBlockoutFloat(0.0f, false, "Z");

	FBlockoutFVector(){}

	FBlockoutFVector(float InX, float InY, float InZ,
		bool InShowTextLabelX=false, FString InTextLabelX=FString("X"),
		bool InShowTextLabelY=false, FString InTextLabelY=FString("Y"),
		bool InShowTextLabelZ=false, FString InTextLabelZ=FString("Z"))
		: X(InX, InShowTextLabelX, InTextLabelX)
		, Y(InY, InShowTextLabelY, InTextLabelY)
		, Z(InZ, InShowTextLabelZ, InTextLabelZ)
	{
	}
	
	FVector ToFVector() const
	{
		return FVector(X.Value, Y.Value, Z.Value);
	}

	FBlockoutFVector& operator=(const FVector& RHS)
	{
		X = RHS.X;
		Y = RHS.Y;
		Z = RHS.Z;
		return *this;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s%s%s"), *X.ToString(), *Y.ToString(), *Z.ToString());
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFVector2D
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat X = FBlockoutFloat(0.0f, false, "X");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat Y = FBlockoutFloat(0.0f, false, "Y");

	FBlockoutFVector2D(){}
	
	FBlockoutFVector2D(float InX, float InY,
		bool InShowTextLabelX=false, FString InTextLabelX=FString("X"),
		bool InShowTextLabelY=false, FString InTextLabelY=FString("Y"))
		: X(InX, InShowTextLabelX, InTextLabelX)
		, Y(InY, InShowTextLabelY, InTextLabelY)
	{
	}

	FVector2D ToFVector2D() const
	{
		return FVector2D(X.Value, Y.Value);
	}

	FBlockoutFVector2D& operator=(const FVector2D& RHS)
	{
		X = RHS.X;
		Y = RHS.Y;
		return *this;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s%s"), *X.ToString(), *Y.ToString());
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFVector4
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat X = FBlockoutFloat(0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat Y = FBlockoutFloat(0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat Z = FBlockoutFloat(0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutFloat W = FBlockoutFloat(0.0f);

	FBlockoutFVector4(){}
	
	FBlockoutFVector4(float InX, float InY, float InZ, float InW,
		bool InShowTextLabelX=false, FString InTextLabelX=FString("X"),
		bool InShowTextLabelY=false, FString InTextLabelY=FString("Y"),
		bool InShowTextLabelZ=false, FString InTextLabelZ=FString("Z"),
		bool InShowTextLabelW=false, FString InTextLabelW=FString("W"))
		: X(InX, InShowTextLabelX, InTextLabelX)
		, Y(InY, InShowTextLabelY, InTextLabelY)
		, Z(InZ, InShowTextLabelZ, InTextLabelZ)
		, W(InW, InShowTextLabelW, InTextLabelW)
	{
	}

	FVector4 ToFVector4() const
	{
		return FVector4(X.Value, Y.Value, Z.Value, W.Value);
	}

	FBlockoutFVector4& operator=(const FVector4& RHS)
	{
		X = RHS.X;
		Y = RHS.Y;
		Z = RHS.Z;
		W = RHS.W;
		return *this;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s%s%s%s"), *X.ToString(), *Y.ToString(), *Z.ToString(), *W.ToString());
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFIntVector
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutInt X = FBlockoutInt(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutInt Y = FBlockoutInt(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutInt Z = FBlockoutInt(0);

	FBlockoutFIntVector(){}
	
	FBlockoutFIntVector(int InX, int InY, int InZ,
		bool InShowTextLabelX=false, FString InTextLabelX=FString("X"),
		bool InShowTextLabelY=false, FString InTextLabelY=FString("Y"),
		bool InShowTextLabelZ=false, FString InTextLabelZ=FString("Z"))
		: X(InX, InShowTextLabelX, InTextLabelX)
		, Y(InY, InShowTextLabelY, InTextLabelY)
		, Z(InZ, InShowTextLabelZ, InTextLabelZ)
	{
	}

	FIntVector ToFIntVector() const
	{
		return FIntVector(X.Value, Y.Value, Z.Value);
	}

	FBlockoutFIntVector& operator=(const FIntVector& RHS)
	{
		X = RHS.X;
		Y = RHS.Y;
		Z = RHS.Z;
		return *this;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s%s%s"), *X.ToString(), *Y.ToString(), *Z.ToString());
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFIntVector2D
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutInt X = FBlockoutInt(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutInt Y = FBlockoutInt(0);

	FBlockoutFIntVector2D(){}
	
	FBlockoutFIntVector2D(int InX, int InY,
		bool InShowTextLabelX=false, FString InTextLabelX=FString("X"),
		bool InShowTextLabelY=false, FString InTextLabelY=FString("Y"))
		: X(InX, InShowTextLabelX, InTextLabelX)
		, Y(InY, InShowTextLabelY, InTextLabelY)
	{
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s%s"), *X.ToString(), *Y.ToString());
	}
};


USTRUCT(BlueprintType)
struct FBlockoutTransform
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin=-360.0f, UIMax=360.0f))
	FRotator Rotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	FBlockoutTransform(){}

	FBlockoutTransform(FVector InLocation, FRotator InRotation, FVector InScale)
		: Location(InLocation)
		, Rotation(InRotation)
		, Scale(InScale)
	{
	}
	
	FTransform ToFTransform() const
	{
		return FTransform(Rotation, Location, Scale);
	}

	FBlockoutTransform& operator=(const FTransform& RHS)
	{
		Location = RHS.GetLocation();
		Rotation = RHS.GetRotation().Rotator();
		Scale = RHS.GetScale3D();
		return *this;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("Location: %s\nRotation: %s\nScale: %s\n"), *Location.ToString(), *Rotation.ToString(), *Scale.ToString());
	}
};

USTRUCT(BlueprintType)
struct FBlockoutIntervalModeVector
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockoutIntervalMode X = EBlockoutIntervalMode::Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockoutIntervalMode Y = EBlockoutIntervalMode::Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockoutIntervalMode Z = EBlockoutIntervalMode::Min;

	FBlockoutIntervalModeVector(){}

	FBlockoutIntervalModeVector(EBlockoutIntervalMode InPivotXOffset, EBlockoutIntervalMode InPivotYOffset, EBlockoutIntervalMode InPivotZOffset)
		: X(InPivotXOffset)
		, Y(InPivotYOffset)
		, Z(InPivotZOffset)
	{
	}
};

USTRUCT(BlueprintType)
struct FBlockoutSingleUVController
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bReversed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin=-1.0f, UIMax=1.0f, ClampMin=-1.0f, ClampMax=1.0f))
	float Offset = 0.0f;

	FBlockoutSingleUVController(){}

	FBlockoutSingleUVController(bool InReversed, float InOffset)
		: bReversed(InReversed)
		, Offset(InOffset)
	{
	}
};

USTRUCT(BlueprintType)
struct FBlockoutMaterialUVController
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutSingleUVController X = FBlockoutSingleUVController(false, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutSingleUVController Y = FBlockoutSingleUVController(false, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBlockoutSingleUVController Z = FBlockoutSingleUVController(false, 0.0f);

	FBlockoutMaterialUVController(){}

	FBlockoutMaterialUVController(FBlockoutSingleUVController InX, FBlockoutSingleUVController InY, FBlockoutSingleUVController InZ)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}
};


USTRUCT(BlueprintType)
struct FBlockoutFace
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Origin = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Normal = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector XAxis = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector YAxis = FVector(0.0f, 0.0f, 1.0f);
};
