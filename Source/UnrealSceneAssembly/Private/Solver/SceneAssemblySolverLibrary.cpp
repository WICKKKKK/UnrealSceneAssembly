#include "Solver/SceneAssemblySolverLibrary.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Math/Quat.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneAssemblySolver, Log, All);

namespace
{
constexpr double MinExtent = 1.0e-4;
constexpr double MinLogInput = 1.0e-8;
constexpr double SquareRatioTolerance = 1.0e-6;

FVector AbsVector(const FVector& Value)
{
	return FVector(FMath::Abs(Value.X), FMath::Abs(Value.Y), FMath::Abs(Value.Z));
}

bool IsFiniteVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool HasUsableExtents(const FVector& Extents)
{
	return IsFiniteVector(Extents) && Extents.X > MinExtent && Extents.Y > MinExtent && Extents.Z > MinExtent;
}

int32 SignLogRatio(double X, double Y)
{
	if (X <= MinLogInput || Y <= MinLogInput)
	{
		return 0;
	}

	const double LogRatio = FMath::Loge(X / Y);
	if (FMath::Abs(LogRatio) <= SquareRatioTolerance)
	{
		return 0;
	}
	return LogRatio > 0.0 ? 1 : -1;
}

double SafeLogRatio(double Target, double Candidate)
{
	return FMath::Loge(FMath::Max(Target, MinLogInput) / FMath::Max(Candidate, MinLogInput));
}

double ClampScore(double Score)
{
	if (!FMath::IsFinite(Score))
	{
		return 0.0;
	}
	return FMath::Clamp(Score, 0.0, 1.0);
}

double GetAxisComponent(const FVector& Value, int32 AxisIndex)
{
	switch (AxisIndex)
	{
	case 0:
		return Value.X;
	case 1:
		return Value.Y;
	default:
		return Value.Z;
	}
}

FQuat QuatFromAxes(const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
{
	FMatrix Matrix = FMatrix::Identity;
	Matrix.SetAxis(0, AxisX);
	Matrix.SetAxis(1, AxisY);
	Matrix.SetAxis(2, AxisZ);
	FQuat Quat(Matrix);
	if (!Quat.IsNormalized())
	{
		Quat.Normalize();
	}
	return Quat;
}

FVector SafeNormalOr(const FVector& Value, const FVector& Fallback)
{
	return Value.GetSafeNormal(UE_SMALL_NUMBER, Fallback);
}

UBoxComponent* FindPreferredBoxComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	TArray<UBoxComponent*> BoxComponents;
	Actor->GetComponents<UBoxComponent>(BoxComponents);

	UBoxComponent* SingleUsableBox = nullptr;
	int32 UsableCount = 0;
	for (UBoxComponent* BoxComponent : BoxComponents)
	{
		if (!BoxComponent || !BoxComponent->IsRegistered())
		{
			continue;
		}

		if (!HasUsableExtents(AbsVector(BoxComponent->GetScaledBoxExtent())))
		{
			continue;
		}

		++UsableCount;
		SingleUsableBox = BoxComponent;

		const FString Name = BoxComponent->GetName().ToLower();
		if (Name.Contains(TEXT("boundingbox")) || Name.Contains(TEXT("bounding_box")) || Name.Contains(TEXT("bbox")))
		{
			return BoxComponent;
		}
	}

	return UsableCount == 1 ? SingleUsableBox : nullptr;
}

bool ExtractSceneFrame(const FSceneOBB& SceneOBB, FVector& OutLocalCenter, FVector& OutHalfExtents, FTransform& OutWorldFrame)
{
	FTransform Frame = SceneOBB.WorldTransform;
	const FVector TransformScale = Frame.GetScale3D();
	const FVector AbsScale = AbsVector(TransformScale);

	OutLocalCenter = SceneOBB.LocalCenter * TransformScale;
	OutHalfExtents = AbsVector(SceneOBB.HalfExtents * AbsScale);

	Frame.SetScale3D(FVector::OneVector);
	FQuat Rotation = Frame.GetRotation();
	if (!Rotation.IsNormalized())
	{
		Rotation.Normalize();
		Frame.SetRotation(Rotation);
	}

	OutWorldFrame = Frame;
	return HasUsableExtents(OutHalfExtents) && IsFiniteVector(OutLocalCenter);
}

struct FRedirectedSceneFrame
{
	FVector HalfExtents = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector BottomCenter = FVector::ZeroVector;
};

bool RedirectSceneFrameToWorldBottom(
	const FVector& SceneLocalCenter,
	const FVector& SceneHalfExtents,
	const FTransform& SceneFrame,
	FRedirectedSceneFrame& OutRedirectedFrame)
{
	const FVector SceneCenter = SceneFrame.TransformPosition(SceneLocalCenter);
	const FVector SceneAxes[3] =
	{
		SceneFrame.GetUnitAxis(EAxis::X),
		SceneFrame.GetUnitAxis(EAxis::Y),
		SceneFrame.GetUnitAxis(EAxis::Z),
	};

	int32 UpAxisIndex = 0;
	double BestUpAlignment = FMath::Abs(SceneAxes[0].Z);
	for (int32 AxisIndex = 1; AxisIndex < 3; ++AxisIndex)
	{
		const double UpAlignment = FMath::Abs(SceneAxes[AxisIndex].Z);
		if (UpAlignment > BestUpAlignment)
		{
			BestUpAlignment = UpAlignment;
			UpAxisIndex = AxisIndex;
		}
	}

	const double UpSign = SceneAxes[UpAxisIndex].Z >= 0.0 ? 1.0 : -1.0;
	const FVector RedirectedZAxis = SceneAxes[UpAxisIndex] * UpSign;

	int32 RemainingAxes[2] = { 0, 0 };
	int32 RemainingCount = 0;
	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		if (AxisIndex != UpAxisIndex)
		{
			RemainingAxes[RemainingCount++] = AxisIndex;
		}
	}

	const FVector RedirectedXAxis = SceneAxes[RemainingAxes[0]];
	FQuat RedirectedRotation = FRotationMatrix::MakeFromXZ(RedirectedXAxis, RedirectedZAxis).ToQuat();
	if (!RedirectedRotation.IsNormalized())
	{
		RedirectedRotation.Normalize();
	}

	const double UpHalfExtent = GetAxisComponent(SceneHalfExtents, UpAxisIndex);
	OutRedirectedFrame.HalfExtents = FVector(
		GetAxisComponent(SceneHalfExtents, RemainingAxes[0]),
		GetAxisComponent(SceneHalfExtents, RemainingAxes[1]),
		UpHalfExtent);
	OutRedirectedFrame.Rotation = RedirectedRotation;
	OutRedirectedFrame.BottomCenter = SceneCenter - RedirectedZAxis * UpHalfExtent;

	return HasUsableExtents(OutRedirectedFrame.HalfExtents)
		&& IsFiniteVector(SceneCenter)
		&& IsFiniteVector(RedirectedXAxis)
		&& IsFiniteVector(RedirectedZAxis)
		&& !OutRedirectedFrame.Rotation.ContainsNaN()
		&& IsFiniteVector(OutRedirectedFrame.BottomCenter);
}

bool ResolveOrientedSceneFrame(
	const FVector& SceneLocalCenter,
	const FVector& SceneHalfExtents,
	const FTransform& SceneFrame,
	const FAssetCandidate& Candidate,
	const FSolverSettings& Settings,
	FRedirectedSceneFrame& OutRedirectedFrame)
{
	if (!Candidate.bHasOrientation)
	{
		return false;
	}

	const FVector SceneCenter = SceneFrame.TransformPosition(SceneLocalCenter);
	const FVector SceneAxes[3] =
	{
		SceneFrame.GetUnitAxis(EAxis::X),
		SceneFrame.GetUnitAxis(EAxis::Y),
		SceneFrame.GetUnitAxis(EAxis::Z),
	};

	FQuat RelativeRotation = QuatFromAxes(
		SafeNormalOr(Candidate.RelativeOrientationX, FVector::ForwardVector),
		SafeNormalOr(Candidate.RelativeOrientationY, FVector::RightVector),
		SafeNormalOr(Candidate.RelativeOrientationZ, FVector::UpVector));
	if (RelativeRotation.ContainsNaN())
	{
		RelativeRotation = Candidate.RelativeOrientation.Quaternion();
	}

	const FQuat ConceptCamera = Settings.ConceptCameraRotation.Quaternion();
	const FQuat Basis = Settings.OrientBasisRotation.Quaternion();
	const FQuat ThumbnailCamera = (Candidate.bHasThumbnailCamera ? Candidate.ThumbnailCameraRotation : Settings.ThumbnailCameraRotation).Quaternion();
	const FQuat BasisInverse = Basis.Inverse();
	const FQuat TargetRotation = (ConceptCamera * BasisInverse * RelativeRotation * Basis * ThumbnailCamera.Inverse()).GetNormalized();

	static const int32 Permutations[6][3] =
	{
		{0, 1, 2},
		{0, 2, 1},
		{1, 0, 2},
		{1, 2, 0},
		{2, 0, 1},
		{2, 1, 0},
	};
	static const int32 Signs[2] = { -1, 1 };

	double BestDistance = UE_DOUBLE_BIG_NUMBER;
	FQuat BestRotation = FQuat::Identity;
	FVector BestHalfExtents = FVector::ZeroVector;
	FVector BestZAxis = FVector::UpVector;

	for (int32 PermutationIndex = 0; PermutationIndex < 6; ++PermutationIndex)
	{
		const int32* Permutation = Permutations[PermutationIndex];
		for (int32 SignXIndex = 0; SignXIndex < 2; ++SignXIndex)
		{
			const int32 SignX = Signs[SignXIndex];
			for (int32 SignYIndex = 0; SignYIndex < 2; ++SignYIndex)
			{
				const int32 SignY = Signs[SignYIndex];
				for (int32 SignZIndex = 0; SignZIndex < 2; ++SignZIndex)
				{
					const int32 SignZ = Signs[SignZIndex];
					const FVector CandidateX = SceneAxes[Permutation[0]] * static_cast<double>(SignX);
					const FVector CandidateY = SceneAxes[Permutation[1]] * static_cast<double>(SignY);
					const FVector CandidateZ = SceneAxes[Permutation[2]] * static_cast<double>(SignZ);
					if (FVector::DotProduct(FVector::CrossProduct(CandidateX, CandidateY), CandidateZ) <= 0.0)
					{
						continue;
					}

					FQuat CandidateRotation = QuatFromAxes(CandidateX, CandidateY, CandidateZ);
					const double Distance = TargetRotation.AngularDistance(CandidateRotation);
					if (Distance < BestDistance)
					{
						BestDistance = Distance;
						BestRotation = CandidateRotation;
						BestHalfExtents = FVector(
							GetAxisComponent(SceneHalfExtents, Permutation[0]),
							GetAxisComponent(SceneHalfExtents, Permutation[1]),
							GetAxisComponent(SceneHalfExtents, Permutation[2]));
						BestZAxis = CandidateZ;
					}
				}
			}
		}
	}

	OutRedirectedFrame.HalfExtents = BestHalfExtents;
	OutRedirectedFrame.Rotation = BestRotation;
	OutRedirectedFrame.BottomCenter = SceneCenter - BestZAxis * BestHalfExtents.Z;

	return BestDistance < UE_DOUBLE_BIG_NUMBER
		&& HasUsableExtents(OutRedirectedFrame.HalfExtents)
		&& IsFiniteVector(SceneCenter)
		&& IsFiniteVector(BestZAxis)
		&& !OutRedirectedFrame.Rotation.ContainsNaN()
		&& IsFiniteVector(OutRedirectedFrame.BottomCenter);
}

float ComputeAlignedIoU(const FVector& SceneHalfExtents, const FVector& AssetHalfExtents, double ScaleFactor)
{
	const FVector ScaledAssetHalf = AssetHalfExtents * ScaleFactor;
	if (!HasUsableExtents(SceneHalfExtents) || !HasUsableExtents(ScaledAssetHalf))
	{
		return 0.0f;
	}

	const double InterX = 2.0 * FMath::Min<double>(SceneHalfExtents.X, ScaledAssetHalf.X);
	const double InterY = 2.0 * FMath::Min<double>(SceneHalfExtents.Y, ScaledAssetHalf.Y);
	const double InterZ = 2.0 * FMath::Min<double>(SceneHalfExtents.Z, ScaledAssetHalf.Z);
	const double InterVolume = InterX * InterY * InterZ;
	const double SceneVolume = 8.0 * SceneHalfExtents.X * SceneHalfExtents.Y * SceneHalfExtents.Z;
	const double AssetVolume = 8.0 * ScaledAssetHalf.X * ScaledAssetHalf.Y * ScaledAssetHalf.Z;
	const double UnionVolume = SceneVolume + AssetVolume - InterVolume;

	if (UnionVolume <= 0.0 || !FMath::IsFinite(UnionVolume))
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Clamp(InterVolume / UnionVolume, 0.0, 1.0));
}

struct FPendingPlacement
{
	FPlacementResult Result;
	float RawSemanticScore = 1.0f;
	float FitScore = 1.0f;
	float GeometryScore = 1.0f;
};

bool BuildPendingPlacement(
	const FSceneOBB& SceneOBB,
	const FAssetCandidate& Candidate,
	const FSolverSettings& Settings,
	FPendingPlacement& OutPending)
{
	FVector SceneLocalCenter;
	FVector SceneHalfExtents;
	FTransform SceneFrame;
	if (!ExtractSceneFrame(SceneOBB, SceneLocalCenter, SceneHalfExtents, SceneFrame))
	{
		return false;
	}

	FRedirectedSceneFrame RedirectedSceneFrame;
	const bool bUseImageOrientation = Settings.OrientMode != ESceneAssemblyOrientMode::Legacy;
	bool bUsedImageOrientation = false;
	bool bHasRedirectedSceneFrame = false;
	if (bUseImageOrientation && Candidate.bHasOrientation)
	{
		bHasRedirectedSceneFrame = ResolveOrientedSceneFrame(SceneLocalCenter, SceneHalfExtents, SceneFrame, Candidate, Settings, RedirectedSceneFrame);
		bUsedImageOrientation = bHasRedirectedSceneFrame;
	}
	if (!bHasRedirectedSceneFrame)
	{
		bHasRedirectedSceneFrame = RedirectSceneFrameToWorldBottom(SceneLocalCenter, SceneHalfExtents, SceneFrame, RedirectedSceneFrame);
		bUsedImageOrientation = false;
	}
	if (!bHasRedirectedSceneFrame)
	{
		return false;
	}

	const FVector AssetHalfExtents = AbsVector(Candidate.BboxHalfExtents);
	if (!HasUsableExtents(AssetHalfExtents))
	{
		return false;
	}

	const int32 SceneRatioSign = SignLogRatio(RedirectedSceneFrame.HalfExtents.X, RedirectedSceneFrame.HalfExtents.Y);
	const int32 AssetRatioSign = SignLogRatio(AssetHalfExtents.X, AssetHalfExtents.Y);
	const bool bSwapHorizontalAxes = !bUsedImageOrientation && (SceneRatioSign * AssetRatioSign) < 0;
	const FVector AlignedAssetHalfExtents = bSwapHorizontalAxes
		? FVector(AssetHalfExtents.Y, AssetHalfExtents.X, AssetHalfExtents.Z)
		: AssetHalfExtents;

	const double Dx = SafeLogRatio(RedirectedSceneFrame.HalfExtents.X, AlignedAssetHalfExtents.X);
	const double Dy = SafeLogRatio(RedirectedSceneFrame.HalfExtents.Y, AlignedAssetHalfExtents.Y);
	const double Dz = SafeLogRatio(RedirectedSceneFrame.HalfExtents.Z, AlignedAssetHalfExtents.Z);
	const double Lambda = Settings.ScaleMode == ESceneAssemblyScaleMode::MatchHeight ? Dz : (Dx + Dy + Dz) / 3.0;
	const double ScaleFactor = FMath::Exp(Lambda);
	if (!FMath::IsFinite(ScaleFactor) || ScaleFactor <= MinLogInput)
	{
		return false;
	}

	const double ResidualX = Dx - Lambda;
	const double ResidualY = Dy - Lambda;
	const double ResidualZ = Dz - Lambda;
	const double AspectResidual = ResidualX * ResidualX + ResidualY * ResidualY + ResidualZ * ResidualZ;

	const double ScaleSensitivity = FMath::Max(0.0f, Settings.ScaleSensitivity);
	const double AspectSensitivity = FMath::Max(0.0f, Settings.AspectSensitivity);
	const double ScaleScore = ClampScore(FMath::Exp(-ScaleSensitivity * Lambda * Lambda));
	const double FitScore = ClampScore(FMath::Exp(-AspectSensitivity * AspectResidual));
	const double GeometryScore = ClampScore(ScaleScore * FitScore);

	const FQuat SceneRotation = RedirectedSceneFrame.Rotation;
	const FQuat LocalYaw(FVector::UpVector, bSwapHorizontalAxes ? HALF_PI : 0.0f);
	const FQuat ActorRotation = (SceneRotation * LocalYaw).GetNormalized();
	const FVector SceneBottomCenter = RedirectedSceneFrame.BottomCenter;
	const FVector AssetBottomCenter = Candidate.BboxCenter + FVector(0.0, 0.0, -AssetHalfExtents.Z);
	const FVector ActorLocation = SceneBottomCenter - ActorRotation.RotateVector(AssetBottomCenter * ScaleFactor);

	FPlacementResult Result;
	Result.AssetPath = Candidate.AssetPath;
	Result.Transform = FTransform(ActorRotation, ActorLocation, FVector(ScaleFactor));
	Result.FitIoU = ComputeAlignedIoU(RedirectedSceneFrame.HalfExtents, AlignedAssetHalfExtents, ScaleFactor);
	Result.ScaleFactor = static_cast<float>(ScaleFactor);
	Result.ScaleScore = static_cast<float>(ScaleScore);
	Result.SemanticScore = Candidate.SemanticScore;
	Result.YawStep = bSwapHorizontalAxes ? 1 : 0;

	OutPending.Result = Result;
	OutPending.RawSemanticScore = Candidate.SemanticScore;
	OutPending.FitScore = static_cast<float>(FitScore);
	OutPending.GeometryScore = static_cast<float>(GeometryScore);
	return true;
}

void ApplyFinalScores(TArray<FPendingPlacement>& PendingPlacements, const FSolverSettings& Settings)
{
	if (PendingPlacements.IsEmpty())
	{
		return;
	}

	float MinSemantic = PendingPlacements[0].RawSemanticScore;
	float MaxSemantic = PendingPlacements[0].RawSemanticScore;
	for (const FPendingPlacement& Pending : PendingPlacements)
	{
		MinSemantic = FMath::Min(MinSemantic, Pending.RawSemanticScore);
		MaxSemantic = FMath::Max(MaxSemantic, Pending.RawSemanticScore);
	}

	const bool bCanNormalizeSemantic = Settings.bNormalizeSemantic && MaxSemantic > MinSemantic + SMALL_NUMBER;
	const double WeightSemantic = FMath::Max(0.0f, Settings.WeightSemantic);
	const double WeightGeometry = FMath::Max(0.0f, Settings.WeightGeometry);

	for (FPendingPlacement& Pending : PendingPlacements)
	{
		float SemanticScore = Pending.RawSemanticScore;
		if (Settings.bNormalizeSemantic)
		{
			SemanticScore = bCanNormalizeSemantic ? (SemanticScore - MinSemantic) / (MaxSemantic - MinSemantic) : 1.0f;
		}
		SemanticScore = static_cast<float>(ClampScore(SemanticScore));

		const double GeometryScore = ClampScore(Pending.GeometryScore);
		double FinalScore = 0.0;
		if (Settings.CombineMode == ESceneAssemblyScoreCombineMode::Additive)
		{
			FinalScore = WeightSemantic * SemanticScore + WeightGeometry * GeometryScore;
		}
		else
		{
			FinalScore = FMath::Pow(static_cast<double>(SemanticScore), WeightSemantic) * FMath::Pow(GeometryScore, WeightGeometry);
		}

		Pending.Result.SemanticScore = SemanticScore;
		Pending.Result.FinalScore = static_cast<float>(FMath::Max(0.0, FinalScore));
	}
}
}

FSceneOBB USceneAssemblySolverLibrary::GetActorOBB(AActor* Actor)
{
	FSceneOBB Result;
	if (!Actor)
	{
		UE_LOG(LogSceneAssemblySolver, Warning, TEXT("GetActorOBB called with a null actor."));
		return Result;
	}

	if (UBoxComponent* BoxComponent = FindPreferredBoxComponent(Actor))
	{
		const FTransform ComponentTransform = BoxComponent->GetComponentTransform();
		Result.LocalCenter = FVector::ZeroVector;
		Result.HalfExtents = AbsVector(BoxComponent->GetScaledBoxExtent());
		Result.WorldTransform = FTransform(ComponentTransform.GetRotation(), ComponentTransform.GetLocation(), FVector::OneVector);
		return Result;
	}

	FBox LocalBox = Actor->CalculateComponentsBoundingBoxInLocalSpace(true, false);
	if (!LocalBox.IsValid)
	{
		LocalBox = Actor->CalculateComponentsBoundingBoxInLocalSpace(false, false);
	}

	if (!LocalBox.IsValid)
	{
		UE_LOG(LogSceneAssemblySolver, Warning, TEXT("Actor has no valid component bounds: %s"), *Actor->GetPathName());
		return Result;
	}

	const FTransform ActorTransform = Actor->GetActorTransform();
	const FVector ActorScale = ActorTransform.GetScale3D();
	const FVector AbsActorScale = AbsVector(ActorScale);
	Result.LocalCenter = LocalBox.GetCenter() * ActorScale;
	Result.HalfExtents = AbsVector(LocalBox.GetExtent() * AbsActorScale);
	Result.WorldTransform = FTransform(ActorTransform.GetRotation(), ActorTransform.GetLocation(), FVector::OneVector);
	return Result;
}

TArray<FPlacementResult> USceneAssemblySolverLibrary::SolvePlacement(const FSceneOBB& SceneOBB, const TArray<FAssetCandidate>& Candidates, const FSolverSettings& Settings)
{
	TArray<FPendingPlacement> PendingPlacements;
	PendingPlacements.Reserve(Candidates.Num());

	for (const FAssetCandidate& Candidate : Candidates)
	{
		FPendingPlacement Pending;
		if (BuildPendingPlacement(SceneOBB, Candidate, Settings, Pending))
		{
			PendingPlacements.Add(Pending);
		}
	}

	ApplyFinalScores(PendingPlacements, Settings);

	TArray<FPlacementResult> Results;
	Results.Reserve(PendingPlacements.Num());
	for (const FPendingPlacement& Pending : PendingPlacements)
	{
		if (Pending.Result.FinalScore >= Settings.FinalScoreThreshold)
		{
			Results.Add(Pending.Result);
		}
	}

	Results.Sort([](const FPlacementResult& A, const FPlacementResult& B)
	{
		if (!FMath::IsNearlyEqual(A.FinalScore, B.FinalScore))
		{
			return A.FinalScore > B.FinalScore;
		}
		if (!FMath::IsNearlyEqual(A.FitIoU, B.FitIoU))
		{
			return A.FitIoU > B.FitIoU;
		}
		return A.AssetPath < B.AssetPath;
	});

	const int32 TopK = FMath::Max(1, Settings.TopK);
	if (Results.Num() > TopK)
	{
		Results.SetNum(TopK);
	}

	return Results;
}

bool USceneAssemblySolverLibrary::RunSolverSelfTest(float& OutFitIoU, FString& OutMessage)
{
	FSceneOBB SceneOBB;
	SceneOBB.LocalCenter = FVector::ZeroVector;
	SceneOBB.HalfExtents = FVector(100.0, 50.0, 25.0);
	SceneOBB.WorldTransform = FTransform(FQuat(FVector::UpVector, PI / 6.0f), FVector(10.0, 20.0, 30.0), FVector::OneVector);

	FAssetCandidate Candidate;
	Candidate.AssetPath = TEXT("/SceneAssembly/SelfTestAsset");
	Candidate.BboxCenter = FVector::ZeroVector;
	Candidate.BboxHalfExtents = FVector(10.0, 5.0, 2.5);
	Candidate.SemanticScore = 1.0f;

	FSolverSettings Settings;
	Settings.ScaleMode = ESceneAssemblyScaleMode::FitIoU;
	Settings.CombineMode = ESceneAssemblyScoreCombineMode::Multiplicative;
	Settings.TopK = 1;

	TArray<FAssetCandidate> Candidates;
	Candidates.Add(Candidate);
	const TArray<FPlacementResult> Results = SolvePlacement(SceneOBB, Candidates, Settings);
	if (Results.IsEmpty())
	{
		OutFitIoU = 0.0f;
		OutMessage = TEXT("Solver returned no self-test result.");
		return false;
	}

	FSceneOBB TippedSceneOBB;
	TippedSceneOBB.LocalCenter = FVector::ZeroVector;
	TippedSceneOBB.HalfExtents = FVector(100.0, 50.0, 25.0);
	TippedSceneOBB.WorldTransform = FTransform(FQuat(FVector::XAxisVector, HALF_PI), FVector(10.0, 20.0, 30.0), FVector::OneVector);

	FAssetCandidate TippedCandidate;
	TippedCandidate.AssetPath = TEXT("/SceneAssembly/SelfTestTippedAsset");
	TippedCandidate.BboxCenter = FVector::ZeroVector;
	TippedCandidate.BboxHalfExtents = FVector(10.0, 2.5, 5.0);
	TippedCandidate.SemanticScore = 1.0f;

	TArray<FAssetCandidate> TippedCandidates;
	TippedCandidates.Add(TippedCandidate);
	const TArray<FPlacementResult> TippedResults = SolvePlacement(TippedSceneOBB, TippedCandidates, Settings);
	if (TippedResults.IsEmpty())
	{
		OutFitIoU = 0.0f;
		OutMessage = TEXT("Solver returned no tipped self-test result.");
		return false;
	}

	FVector TippedSceneLocalCenter;
	FVector TippedSceneHalfExtents;
	FTransform TippedSceneFrame;
	FRedirectedSceneFrame ExpectedTippedFrame;
	const bool bHasExpectedTippedFrame = ExtractSceneFrame(TippedSceneOBB, TippedSceneLocalCenter, TippedSceneHalfExtents, TippedSceneFrame)
		&& RedirectSceneFrameToWorldBottom(TippedSceneLocalCenter, TippedSceneHalfExtents, TippedSceneFrame, ExpectedTippedFrame);

	const FVector TippedActorUpAxis = TippedResults[0].Transform.GetUnitAxis(EAxis::Z);
	const FVector TippedAssetBottomCenter = TippedCandidate.BboxCenter + FVector(0.0, 0.0, -TippedCandidate.BboxHalfExtents.Z);
	const FVector TippedPlacedBottomCenter = TippedResults[0].Transform.TransformPosition(TippedAssetBottomCenter);
	const bool bTippedPass = bHasExpectedTippedFrame
		&& TippedResults[0].FitIoU >= 0.999f
		&& FMath::IsNearlyEqual(TippedResults[0].ScaleFactor, 10.0f, 1.0e-3f)
		&& FVector::DotProduct(TippedActorUpAxis, ExpectedTippedFrame.Rotation.GetAxisZ()) >= 0.999f
		&& FVector::Dist(TippedPlacedBottomCenter, ExpectedTippedFrame.BottomCenter) <= 1.0e-3f;

	FSceneOBB OrientedSceneOBB;
	OrientedSceneOBB.LocalCenter = FVector::ZeroVector;
	OrientedSceneOBB.HalfExtents = FVector(100.0, 50.0, 25.0);
	OrientedSceneOBB.WorldTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::OneVector);

	FAssetCandidate OrientedCandidate;
	OrientedCandidate.AssetPath = TEXT("/SceneAssembly/SelfTestOrientedAsset");
	OrientedCandidate.BboxCenter = FVector::ZeroVector;
	OrientedCandidate.BboxHalfExtents = FVector(10.0, 2.5, 5.0);
	OrientedCandidate.SemanticScore = 1.0f;
	OrientedCandidate.bHasOrientation = true;
	OrientedCandidate.RelativeOrientationX = FVector::ForwardVector;
	OrientedCandidate.RelativeOrientationY = FVector::UpVector;
	OrientedCandidate.RelativeOrientationZ = -FVector::RightVector;

	FSolverSettings OrientedSettings = Settings;
	OrientedSettings.OrientMode = ESceneAssemblyOrientMode::Precomputed;
	TArray<FAssetCandidate> OrientedCandidates;
	OrientedCandidates.Add(OrientedCandidate);
	const TArray<FPlacementResult> OrientedResults = SolvePlacement(OrientedSceneOBB, OrientedCandidates, OrientedSettings);
	const bool bOrientedPass = !OrientedResults.IsEmpty()
		&& FVector::DotProduct(OrientedResults[0].Transform.GetUnitAxis(EAxis::Z), -FVector::RightVector) >= 0.999f;

	OutFitIoU = FMath::Min(Results[0].FitIoU, TippedResults[0].FitIoU);
	const bool bPass = OutFitIoU >= 0.999f
		&& FMath::IsNearlyEqual(Results[0].ScaleFactor, 10.0f, 1.0e-3f)
		&& bTippedPass
		&& bOrientedPass;
	OutMessage = bPass
		? TEXT("Solver self-test passed.")
		: FString::Printf(
			TEXT("Solver self-test failed: UprightIoU=%.6f UprightScale=%.6f TippedIoU=%.6f TippedScale=%.6f TippedUpDot=%.6f TippedBottomError=%.6f OrientedPass=%d"),
			Results[0].FitIoU,
			Results[0].ScaleFactor,
			TippedResults[0].FitIoU,
			TippedResults[0].ScaleFactor,
			bHasExpectedTippedFrame ? FVector::DotProduct(TippedActorUpAxis, ExpectedTippedFrame.Rotation.GetAxisZ()) : 0.0,
			bHasExpectedTippedFrame ? FVector::Dist(TippedPlacedBottomCenter, ExpectedTippedFrame.BottomCenter) : 0.0,
			bOrientedPass ? 1 : 0);
	return bPass;
}
