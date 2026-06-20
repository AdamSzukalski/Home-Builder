#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;


UCLASS()
class HOMEBUILDER_API ACameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ACameraPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	// Input Handlers
	void Input_Pan(const FInputActionValue& Value);
	void Input_ZoomAxis(const FInputActionValue& Value);
	void Input_RotateStart(const FInputActionValue& Value);
	void Input_RotateStop(const FInputActionValue& Value);
	void Input_RotateAxis(const FInputActionValue& Value);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// Enhanced Input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Pan;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Zoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_RotateTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_RotateAxis;

	// Tuning: Pan
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan")
	float PanSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan")
	float PanSmoothSpeed = 8.0f;

	// Tuning: Zoom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomStep = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomMin = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomMax = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomSmoothSpeed = 8.0f;

	// Tuning: Orbit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float OrbitSensitivity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float PitchMin = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float PitchMax = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float OrbitSmoothSpeed = 12.0f;

private:
	// Raw Input Accumulation
	FVector2D PanInput = FVector2D::ZeroVector;
	float ZoomInput = 0.0f;
	FVector2D OrbitInput = FVector2D::ZeroVector;
	bool bIsOrbiting = false;

	//Interpolation Targets
	FVector TargetLocation;
	float TargetArmLength;
	float TargetYaw;
	float TargetPitch;
	
};
