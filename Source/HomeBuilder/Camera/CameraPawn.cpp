#include "CameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Math/UnrealMathUtility.h"


ACameraPawn::ACameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Spring Arm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->TargetArmLength = 1200.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	
	// Start at default angle
	SpringArm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

}

void ACameraPawn::BeginPlay()
{
	Super::BeginPlay();

	TargetLocation = GetActorLocation();
	TargetArmLength = SpringArm->TargetArmLength;
	TargetYaw = GetActorRotation().Yaw;
	TargetPitch = SpringArm->GetRelativeRotation().Pitch;

	// Register enhanced input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(CameraMappingContext, 0);
		}
	}
	
}

void ACameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	
	EnhancedInputComponent->BindAction(
		IA_Pan, ETriggerEvent::Triggered, this, &ACameraPawn::Input_Pan);
	EnhancedInputComponent->BindAction(
		IA_Zoom, ETriggerEvent::Triggered, this, &ACameraPawn::Input_ZoomAxis);
	EnhancedInputComponent->BindAction(
		IA_RotateTrigger, ETriggerEvent::Started, this, &ACameraPawn::Input_RotateStart);
	EnhancedInputComponent->BindAction(
		IA_RotateTrigger, ETriggerEvent::Completed, this, &ACameraPawn::Input_RotateStop);
	EnhancedInputComponent->BindAction(
		IA_RotateAxis, ETriggerEvent::Triggered, this, &ACameraPawn::Input_RotateAxis);
	
}

// Called to bind functionality to input
void ACameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Pan: move target along world XY plane
	if (!PanInput.IsNearlyZero())
	{
		// Orient pan direction relative to current camera yaw so WASD feels natural
		const FRotator YawRotation = FRotator(0.0f, TargetYaw, 0.0f);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		TargetLocation += ForwardDirection * PanInput.Y * PanSpeed * DeltaTime;
		TargetLocation += RightDirection * PanInput.X * PanSpeed * DeltaTime;
		PanInput = FVector2D::ZeroVector;
	}

	// Orbit: accumulate mouse delta while RMB held;
	if (bIsOrbiting && !OrbitInput.IsNearlyZero())
	{
		TargetYaw += OrbitInput.X * OrbitSensitivity;
		TargetPitch = FMath::Clamp(TargetPitch + OrbitInput.Y * OrbitSensitivity, -PitchMax, -PitchMin);
		OrbitInput = FVector2D::ZeroVector;
	}

	// Zoom: clamp target arm length
	TargetArmLength = FMath::Clamp(TargetArmLength + ZoomInput, ZoomMin, ZoomMax);
	ZoomInput = 0.0f;

	// Smooth interpolation toward all targets
	const FVector SmoothedLocation = FMath::VInterpTo(
		GetActorLocation(), TargetLocation, DeltaTime, PanSmoothSpeed);
	const float SmoothedArm = FMath::FInterpTo(
		SpringArm->TargetArmLength, TargetArmLength, DeltaTime, ZoomSmoothSpeed);
	float CurrentYaw = GetActorRotation().Yaw;
	float YawDelta = FMath::UnwindDegrees(TargetYaw - CurrentYaw);
	const float SmoothedYaw = CurrentYaw + YawDelta * FMath::Clamp(OrbitSmoothSpeed * DeltaTime, 0.f, 1.f);
	const float SmoothedPitch = FMath::FInterpTo(
		SpringArm->GetRelativeRotation().Pitch, TargetPitch, DeltaTime, OrbitSmoothSpeed);

	SetActorLocation(SmoothedLocation);
	SetActorRotation(FRotator(0.0f, SmoothedYaw, 0.0f));
	SpringArm->SetRelativeRotation(FRotator(SmoothedPitch, 0.0f, 0.0f));
	SpringArm->TargetArmLength = SmoothedArm;
}

// Input Handlers
void ACameraPawn::Input_Pan(const FInputActionValue& Value)
{
	PanInput = Value.Get<FVector2D>();
}

void ACameraPawn::Input_ZoomAxis(const FInputActionValue& Value)
{
	// Positive scroll = zoom in (shorter arm)
	ZoomInput = -Value.Get<float>() * ZoomStep;
}

void ACameraPawn::Input_RotateStart(const FInputActionValue& Value)
{
	bIsOrbiting = true;
}
void ACameraPawn::Input_RotateStop(const FInputActionValue& Value)
{
	bIsOrbiting = false;
}
void ACameraPawn::Input_RotateAxis(const FInputActionValue& Value)
{
	if (bIsOrbiting)
	{
		OrbitInput = Value.Get<FVector2D>();
	}
}

