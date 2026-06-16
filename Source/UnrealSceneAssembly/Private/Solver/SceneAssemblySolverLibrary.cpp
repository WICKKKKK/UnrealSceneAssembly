#include "Solver/SceneAssemblySolverLibrary.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Math/Quat.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneAssemblySolver, Log, All);

namespace
{
constexpr double MinExtent = 1.0e-4;
constexpr double MinLogInput = 1.0e-8;
constexpr double SquareRatioTolerance = 1.0e-6;
constexpr float DefaultThumbnailOrbitPitch = -11.25f;
constexpr float DefaultThumbnailOrbitYaw = -157.5f;

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

// --- Dual Image geometry helpers --------------------------------------------
// All matrices here use the standard math / column-vector active convention
// (v' = R * v), matching the Orient-Anything Python reference (_pose_matrix).
// This is intentionally NOT FMatrix (which is row-vector); we only convert to
// FQuat at the very end so the column convention stays internally consistent.

struct FMat3
{
	double M[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

	static FMat3 Identity()
	{
		return FMat3();
	}
};

FMat3 Mat3Mul(const FMat3& A, const FMat3& B)
{
	FMat3 R;
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			R.M[i][j] = A.M[i][0] * B.M[0][j] + A.M[i][1] * B.M[1][j] + A.M[i][2] * B.M[2][j];
		}
	}
	return R;
}

double Mat3Determinant(const FMat3& A)
{
	return A.M[0][0] * (A.M[1][1] * A.M[2][2] - A.M[1][2] * A.M[2][1])
		- A.M[0][1] * (A.M[1][0] * A.M[2][2] - A.M[1][2] * A.M[2][0])
		+ A.M[0][2] * (A.M[1][0] * A.M[2][1] - A.M[1][1] * A.M[2][0]);
}

FMat3 Mat3SwapColumns01(const FMat3& A)
{
	FMat3 R = A;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		R.M[Row][0] = A.M[Row][1];
		R.M[Row][1] = A.M[Row][0];
	}
	return R;
}

FMat3 Mat3RotX(double Rad)
{
	const double c = FMath::Cos(Rad);
	const double s = FMath::Sin(Rad);
	FMat3 R;
	R.M[0][0] = 1; R.M[0][1] = 0; R.M[0][2] = 0;
	R.M[1][0] = 0; R.M[1][1] = c; R.M[1][2] = -s;
	R.M[2][0] = 0; R.M[2][1] = s; R.M[2][2] = c;
	return R;
}

FMat3 Mat3RotY(double Rad)
{
	const double c = FMath::Cos(Rad);
	const double s = FMath::Sin(Rad);
	FMat3 R;
	R.M[0][0] = c;  R.M[0][1] = 0; R.M[0][2] = s;
	R.M[1][0] = 0;  R.M[1][1] = 1; R.M[1][2] = 0;
	R.M[2][0] = -s; R.M[2][1] = 0; R.M[2][2] = c;
	return R;
}

FMat3 Mat3RotZ(double Rad)
{
	const double c = FMath::Cos(Rad);
	const double s = FMath::Sin(Rad);
	FMat3 R;
	R.M[0][0] = c; R.M[0][1] = -s; R.M[0][2] = 0;
	R.M[1][0] = s; R.M[1][1] = c;  R.M[1][2] = 0;
	R.M[2][0] = 0; R.M[2][1] = 0;  R.M[2][2] = 1;
	return R;
}

// Orient-Anything object pose: Obj(az, polar, rot) = Rx(rot) * Ry(polar) * Rz(-az).
// Matches Python _pose_matrix and the upstream azi_ele_rot_to_Obj_Rmatrix.
FMat3 OrientPoseMatrix(double AzimuthDeg, double PolarDeg, double RotationDeg)
{
	const double Az = FMath::DegreesToRadians(AzimuthDeg);
	const double El = FMath::DegreesToRadians(PolarDeg);
	const double Ro = FMath::DegreesToRadians(RotationDeg);
	return Mat3Mul(Mat3RotX(Ro), Mat3Mul(Mat3RotY(El), Mat3RotZ(-Az)));
}

static constexpr int32 SingleImageBasisPermutationCount = 6;
static constexpr int32 SingleImageBasisSignCount = 8;
static constexpr int32 SingleImageBasisCandidateCount = SingleImageBasisPermutationCount * SingleImageBasisSignCount;

static constexpr int32 SingleImageBasisPermutations[SingleImageBasisPermutationCount][3] =
{
	{0, 1, 2}, // XYZ
	{0, 2, 1}, // XZY
	{1, 0, 2}, // YXZ
	{1, 2, 0}, // YZX
	{2, 0, 1}, // ZXY
	{2, 1, 0}, // ZYX
};

static constexpr const TCHAR* SingleImageBasisPermutationLabels[SingleImageBasisPermutationCount] =
{
	TEXT("XYZ"),
	TEXT("XZY"),
	TEXT("YXZ"),
	TEXT("YZX"),
	TEXT("ZXY"),
	TEXT("ZYX"),
};

int32 GetClampedSingleImageBasisCandidateIndex(int32 BasisCandidateIndex)
{
	return FMath::Clamp(BasisCandidateIndex, 0, SingleImageBasisCandidateCount - 1);
}

FMat3 SingleImageBasisCandidate(int32 BasisCandidateIndex)
{
	const int32 ClampedIndex = GetClampedSingleImageBasisCandidateIndex(BasisCandidateIndex);
	const int32 PermutationIndex = ClampedIndex / SingleImageBasisSignCount;
	const int32 SignMask = ClampedIndex % SingleImageBasisSignCount;
	FMat3 R;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			R.M[Row][Column] = 0.0;
		}
	}
	for (int32 Column = 0; Column < 3; ++Column)
	{
		const int32 Row = SingleImageBasisPermutations[PermutationIndex][Column];
		const bool bFlip = (SignMask & (1 << (2 - Column))) != 0;
		R.M[Row][Column] = bFlip ? -1.0 : 1.0;
	}
	return R;
}

FVector Mat3Column(const FMat3& Matrix, int32 ColumnIndex)
{
	return FVector(Matrix.M[0][ColumnIndex], Matrix.M[1][ColumnIndex], Matrix.M[2][ColumnIndex]);
}

// Convert a column-vector active rotation matrix (v' = R * v) to an FQuat whose
// RotateVector matches R. UE FQuat(FMatrix) expects a row-vector matrix (rows are
// basis axes), which equals R^T, so we feed the columns of R as the FMatrix rows.
FQuat QuatFromColumnMatrix(const FMat3& R)
{
	FMatrix Matrix = FMatrix::Identity;
	Matrix.SetAxis(0, FVector(R.M[0][0], R.M[1][0], R.M[2][0]));
	Matrix.SetAxis(1, FVector(R.M[0][1], R.M[1][1], R.M[2][1]));
	Matrix.SetAxis(2, FVector(R.M[0][2], R.M[1][2], R.M[2][2]));
	FQuat Quat(Matrix);
	if (!Quat.IsNormalized())
	{
		Quat.Normalize();
	}
	return Quat;
}

FMat3 ImagePoseCameraLocalAxes(const FVector& OrientPoseDeg, int32 BasisCandidateIndex, bool bSwapFrontRight)
{
	const FMat3 Basis = SingleImageBasisCandidate(BasisCandidateIndex);
	const FMat3 ObjectPose = OrientPoseMatrix(OrientPoseDeg.X, OrientPoseDeg.Y, OrientPoseDeg.Z);
	FMat3 CameraLocalAxes = Mat3Mul(Basis, ObjectPose);
	if (bSwapFrontRight)
	{
		CameraLocalAxes = Mat3SwapColumns01(CameraLocalAxes);
	}
	return CameraLocalAxes;
}

FQuat ComputeImageObjectWorldRotationQuat(const FVector& OrientPoseDeg, const FRotator& CameraRotation, int32 BasisCandidateIndex, bool bSwapFrontRight)
{
	const FMat3 CameraLocalAxes = ImagePoseCameraLocalAxes(OrientPoseDeg, BasisCandidateIndex, bSwapFrontRight);
	const FQuat CameraLocalRotation = QuatFromColumnMatrix(CameraLocalAxes);
	return (CameraRotation.Quaternion() * CameraLocalRotation).GetNormalized();
}

FRotator DefaultThumbnailCameraRotation()
{
	float OrbitPitch = DefaultThumbnailOrbitPitch;
	float OrbitYaw = DefaultThumbnailOrbitYaw;
#if WITH_EDITOR
	if (const USceneThumbnailInfo* ThumbnailInfo = GetDefault<USceneThumbnailInfo>())
	{
		OrbitPitch = ThumbnailInfo->OrbitPitch;
		OrbitYaw = ThumbnailInfo->OrbitYaw;
	}
#endif

	// Mirrors ThumbnailLibrary.cpp::ThumbnailCameraRotationFromOrbit and UE's scene thumbnail view.
	const FRotator RotationOffsetToViewCenter(0.0f, 90.0f, 0.0f);
	FMatrix ThumbnailViewRotation = FRotationMatrix(FRotator(0.0f, OrbitYaw, 0.0f))
		* FRotationMatrix(FRotator(0.0f, 0.0f, OrbitPitch))
		* FInverseRotationMatrix(RotationOffsetToViewCenter);
	ThumbnailViewRotation = ThumbnailViewRotation.RemoveTranslation();

	FMatrix CameraToLocal = ThumbnailViewRotation.InverseFast();
	CameraToLocal.RemoveScaling();
	FQuat Rotation(CameraToLocal);
	if (!Rotation.IsNormalized())
	{
		Rotation.Normalize();
	}
	return Rotation.Rotator();
}

// Dual Image uses the validated single-image mapping for both images. target_abs
// gives the scene-observed object rotation; ref gives the thumbnail-observed
// residual of an asset rendered at authored identity. Apply the residual in world
// rotation space: Actor = R_target * R_ref^-1.
FQuat ResolveDualImageWorldRotationQuat(const FAssetCandidate& Candidate, const FSolverSettings& Settings)
{
	const int32 BasisCandidateIndex = GetClampedSingleImageBasisCandidateIndex(Settings.OrientBasisCandidateIndex);
	const bool bSwapFrontRight = Settings.bOrientSwapFrontRight;
	const FQuat TargetWorld = ComputeImageObjectWorldRotationQuat(
		Candidate.TargetOrientationPose,
		Settings.ConceptCameraRotation,
		BasisCandidateIndex,
		bSwapFrontRight);

	if (!Candidate.bHasRefPose)
	{
		return TargetWorld;
	}

	const FRotator ThumbnailCamera = Candidate.bHasThumbnailCamera
		? Candidate.ThumbnailCameraRotation
		: DefaultThumbnailCameraRotation();
	const FQuat RefWorld = ComputeImageObjectWorldRotationQuat(
		Candidate.RefOrientationPose,
		ThumbnailCamera,
		BasisCandidateIndex,
		bSwapFrontRight);
	return (TargetWorld * RefWorld.Inverse()).GetNormalized();
}

FQuat ResolveImageOrientationWorldRotationQuat(const FAssetCandidate& Candidate, const FSolverSettings& Settings)
{
	// Dual Image maps ref/target_abs through the same calibrated single-image basis,
	// then removes the ref residual introduced by Orient's thumbnail prediction.
	if (Settings.OrientMode == ESceneAssemblyOrientMode::DualImage && Candidate.bHasTargetPose && Candidate.bHasRefPose)
	{
		return ResolveDualImageWorldRotationQuat(Candidate, Settings);
	}

	return FQuat::Identity;
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

	const FQuat TargetRotation = ResolveImageOrientationWorldRotationQuat(Candidate, Settings);

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
	const bool bUseImageOrientation = Settings.OrientMode == ESceneAssemblyOrientMode::DualImage;
	bool bUsedImageOrientation = false;
	bool bHasRedirectedSceneFrame = false;
	if (bUseImageOrientation && Candidate.bHasOrientation && Candidate.bHasTargetPose && Candidate.bHasRefPose)
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

FRotator USceneAssemblySolverLibrary::ResolveImageOrientationWorldRotation(const FAssetCandidate& Candidate, const FSolverSettings& Settings)
{
	if (!Candidate.bHasOrientation)
	{
		return FRotator::ZeroRotator;
	}

	const FQuat Rotation = ResolveImageOrientationWorldRotationQuat(Candidate, Settings);
	return Rotation.ContainsNaN() ? FRotator::ZeroRotator : Rotation.Rotator();
}

int32 USceneAssemblySolverLibrary::GetSingleImageBasisCandidateCount()
{
	return SingleImageBasisCandidateCount;
}

FString USceneAssemblySolverLibrary::GetSingleImageBasisCandidateLabel(int32 BasisCandidateIndex)
{
	const int32 ClampedIndex = GetClampedSingleImageBasisCandidateIndex(BasisCandidateIndex);
	const int32 PermutationIndex = ClampedIndex / SingleImageBasisSignCount;
	const int32 SignMask = ClampedIndex % SingleImageBasisSignCount;
	return FString::Printf(
		TEXT("%s signs(%c,%c,%c)"),
		SingleImageBasisPermutationLabels[PermutationIndex],
		(SignMask & 4) ? TEXT('-') : TEXT('+'),
		(SignMask & 2) ? TEXT('-') : TEXT('+'),
		(SignMask & 1) ? TEXT('-') : TEXT('+'));
}

FRotator USceneAssemblySolverLibrary::ComputeSingleImageWorldRotation(
	const FVector& OrientPoseDeg,
	const FRotator& CameraRotation,
	int32 BasisCandidateIndex,
	bool bSwapFrontRight)
{
	const FQuat WorldRotation = ComputeImageObjectWorldRotationQuat(OrientPoseDeg, CameraRotation, BasisCandidateIndex, bSwapFrontRight);
	return WorldRotation.ContainsNaN() ? FRotator::ZeroRotator : WorldRotation.Rotator();
}

FRotator USceneAssemblySolverLibrary::GetDefaultThumbnailCameraRotation()
{
	return DefaultThumbnailCameraRotation();
}

void USceneAssemblySolverLibrary::ComputeSingleImageWorldAxes(
	const FVector& OrientPoseDeg,
	const FRotator& CameraRotation,
	int32 BasisCandidateIndex,
	bool bSwapFrontRight,
	FVector& OutFrontWorld,
	FVector& OutRightWorld,
	FVector& OutUpWorld)
{
	const FMat3 CameraLocalAxes = ImagePoseCameraLocalAxes(OrientPoseDeg, BasisCandidateIndex, bSwapFrontRight);

	OutFrontWorld = CameraRotation.RotateVector(Mat3Column(CameraLocalAxes, 0)).GetSafeNormal();
	OutRightWorld = CameraRotation.RotateVector(Mat3Column(CameraLocalAxes, 1)).GetSafeNormal();
	OutUpWorld = CameraRotation.RotateVector(Mat3Column(CameraLocalAxes, 2)).GetSafeNormal();
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

	// Dual Image math self-test: pin down the column-vector matrix convention,
	// verify the validated single-image basis stays a proper rotation, and assert
	// the ref-corrected world rotation on hand-computed physical anchors.
	bool bDualImagePass = true;
	FString DualImageDiag;
	{
		// 1) QuatFromColumnMatrix must reproduce v' = R * v. Use Rz(90deg): maps +X -> +Y.
		const FMat3 RotZ90 = Mat3RotZ(HALF_PI);
		const FQuat RotZ90Quat = QuatFromColumnMatrix(RotZ90);
		const FVector Rotated = RotZ90Quat.RotateVector(FVector(1.0, 0.0, 0.0));
		const bool bColumnConventionPass = FVector::Dist(Rotated, FVector(0.0, 1.0, 0.0)) <= 1.0e-4f;

		// 2) The validated basis is a reflection and swapFR turns it into a proper rotation.
		const FMat3 Basis = SingleImageBasisCandidate(4);
		const double BasisDet = Mat3Determinant(Basis);
		const bool bBasisReflectionPass = FMath::IsNearlyEqual(BasisDet, -1.0, 1.0e-6);
		const FMat3 CanonAxes = ImagePoseCameraLocalAxes(FVector::ZeroVector, 4, true);
		const double CanonDet = Mat3Determinant(CanonAxes);
		const bool bCanonAxesPass = FMath::IsNearlyEqual(CanonDet, 1.0, 1.0e-6);

		// 3) Any pose through the validated Phi mapping must be a proper rotation.
		const FMat3 PoseAxes = ImagePoseCameraLocalAxes(FVector(37.0, 12.0, -8.0), 4, true);
		const double PoseAxesDet = Mat3Determinant(PoseAxes);
		const bool bPoseAxesPass = FMath::IsNearlyEqual(PoseAxesDet, 1.0, 1.0e-6);

		// Physical anchors for World = C_scene*Phi(target_abs) * (C_thumb*Phi(ref))^-1.
		auto DualImageWorld = [](const FRotator& SceneCamera, const FVector& TargetPose)
		{
			FAssetCandidate Candidate;
			Candidate.bHasOrientation = true;
			Candidate.bHasRefPose = true;
			Candidate.RefOrientationPose = FVector::ZeroVector;
			Candidate.bHasThumbnailCamera = true;
			Candidate.ThumbnailCameraRotation = FRotator::ZeroRotator;
			Candidate.bHasTargetPose = true;
			Candidate.TargetOrientationPose = TargetPose;
			FSolverSettings LocalSettings;
			LocalSettings.OrientMode = ESceneAssemblyOrientMode::DualImage;
			LocalSettings.ConceptCameraRotation = SceneCamera;
			LocalSettings.OrientBasisCandidateIndex = 4;
			LocalSettings.bOrientSwapFrontRight = true;
			return ResolveDualImageWorldRotationQuat(Candidate, LocalSettings);
		};

		// 4) Canonical ref/target with identity cameras -> identity after ref correction.
		const FQuat CanonWorld = DualImageWorld(FRotator::ZeroRotator, FVector::ZeroVector);
		const bool bCanonicalPass = CanonWorld.Equals(FQuat::Identity, 1.0e-4f);

		// 5) Roll-only target (rotation=90) rolls about Unreal forward (+X): +Y -> +Z.
		const FQuat RollWorld = DualImageWorld(FRotator::ZeroRotator, FVector(0.0, 0.0, 90.0));
		const FVector RolledUp = RollWorld.RotateVector(FVector(0.0, 1.0, 0.0));
		const bool bRollAxisPass = FVector::Dist(RolledUp, FVector(0.0, 0.0, 1.0)) <= 1.0e-3f;

		// 6) Canonical target under a yawed scene camera -> camera rotation, after
		// removing the identity-camera thumbnail residual.
		const FRotator YawCamera(0.0, 30.0, 0.0);
		const FQuat YawWorld = DualImageWorld(YawCamera, FVector::ZeroVector);
		const bool bCameraComposePass = YawWorld.Equals(YawCamera.Quaternion(), 1.0e-4f);

		bDualImagePass = bColumnConventionPass && bBasisReflectionPass && bCanonAxesPass && bPoseAxesPass
			&& bCanonicalPass && bRollAxisPass && bCameraComposePass;
		DualImageDiag = FString::Printf(
			TEXT("ColumnConv=%d BasisDet=%.6f CanonDet=%.6f PoseDet=%.6f Canon=%d RollAxis=%d CamCompose=%d"),
			bColumnConventionPass ? 1 : 0, BasisDet, CanonDet, PoseAxesDet,
			bCanonicalPass ? 1 : 0, bRollAxisPass ? 1 : 0, bCameraComposePass ? 1 : 0);
	}

	OutFitIoU = FMath::Min(Results[0].FitIoU, TippedResults[0].FitIoU);
	const bool bPass = OutFitIoU >= 0.999f
		&& FMath::IsNearlyEqual(Results[0].ScaleFactor, 10.0f, 1.0e-3f)
		&& bTippedPass
		&& bDualImagePass;
	OutMessage = bPass
		? TEXT("Solver self-test passed.")
		: FString::Printf(
			TEXT("Solver self-test failed: UprightIoU=%.6f UprightScale=%.6f TippedIoU=%.6f TippedScale=%.6f TippedUpDot=%.6f TippedBottomError=%.6f DualImage[%s]"),
			Results[0].FitIoU,
			Results[0].ScaleFactor,
			TippedResults[0].FitIoU,
			TippedResults[0].ScaleFactor,
			bHasExpectedTippedFrame ? FVector::DotProduct(TippedActorUpAxis, ExpectedTippedFrame.Rotation.GetAxisZ()) : 0.0,
			bHasExpectedTippedFrame ? FVector::Dist(TippedPlacedBottomCenter, ExpectedTippedFrame.BottomCenter) : 0.0,
			*DualImageDiag);
	return bPass;
}
