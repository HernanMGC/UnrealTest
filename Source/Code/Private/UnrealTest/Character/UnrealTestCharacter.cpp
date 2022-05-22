// Copyright Epic Games, Inc. All Rights Reserved.

// Unreal Engine
#include "UnrealTest/Character/UnrealTestCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"


// Game Project
#include "UnrealTest/Weapons/BaseWeapon.h"
#include "UnrealTest/Components/HealthComponent.h"
#include "UnrealTest/Game/UnrealTestGameMode.h"
#include "UnrealTest/UI/UnrealTestHUD.h"
#include "UnrealTest/UI/HealthBarWidget.h"

#pragma region Initialization
// Constructor.
AUnrealTestCharacter::AUnrealTestCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetIsReplicated(true);

	// set our turn rate for input
	TurnRateGamepad = TURN_RATE_GAMEPAD;

	// Setup character movement
	ConfigureCharacterMovement(GetCharacterMovement());
	

	// Initialize components
	InitializeCameraBoom();
	InitializeFollowCamera();
	InitializeWeaponHolder();
	InitializeHealthComponent();

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

// Initialize Camera Boom.
void AUnrealTestCharacter::InitializeCameraBoom()
{
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// The camera follows at this distance behind the character	

	CameraBoom->TargetArmLength = 400.0f;

	// Rotate the arm based on the controller
	CameraBoom->bUsePawnControlRotation = true;
}

// Initialize Follow Camera.
void AUnrealTestCharacter::InitializeFollowCamera()
{
	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));

	// Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Camera does not rotate relative to arm
	FollowCamera->bUsePawnControlRotation = false;
}

// Initialize Weapon Holder.
void AUnrealTestCharacter::InitializeWeaponHolder()
{
	// Create a follow camera
	WeaponHolder = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponHolder"));
	WeaponHolder->SetupAttachment(GetMesh());
	WeaponHolder->SetIsReplicated(true);
}

// Initialize Health Component.
void AUnrealTestCharacter::InitializeHealthComponent()
{
	// Creates Health component
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->SetIsReplicated(true);

	// If character is simulated attach Health Widget component
	if (!IsLocallyControlled())
	{
		HealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComponent"));
		HealthWidgetComponent->SetupAttachment(GetMesh());
	}
}
#pragma endregion Initialization

#pragma region Overrides
// Called when the game starts or when spawned.
void AUnrealTestCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Spawn weapon
	FActorSpawnParameters params;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;


	// Attach weapon to weapon holder
	FVector location = FVector();
	FRotator rotation = FRotator();
	CurrentWeapon = Cast<ABaseWeapon>(GetWorld()->SpawnActor(InitialWeaponTemplate, &location, &rotation, params));
	CurrentWeapon->AttachToComponent(WeaponHolder, FAttachmentTransformRules::KeepRelativeTransform);
	CurrentWeapon->SetOwner(this);

	// Set Player HUD reference
	PlayerHUD = Cast<AUnrealTestHUD>(UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD());

	// If Authority level bind OnGameOver event
	if (GetLocalRole() >= ROLE_Authority) {
		HealthComponent->InitializeHealth(MaxHealth);
		AUnrealTestGameMode* gameMode = Cast<AUnrealTestGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
		gameMode->OnGameOver.AddUniqueDynamic(this, &AUnrealTestCharacter::Client_GameOver);
		UE_LOG(LogTemp, Warning, TEXT("AMultiplayerTechTestCharacter::BeginPlay [LocalRole:%s]: HealthInitlalzed "), *((GetLocalRole() >= ROLE_Authority) ? FString("Auth") : FString("NoAuth")));
	}
	
	// If locally controlled set health bar widget
	if (IsLocallyControlled())
	{
		PlayerHUD->SetHealthComponent(HealthComponent);
	}
	// If NOT locally controlled set WorldSpace healthbar
	else
	{
		APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		UHealthBarWidget* healthWidget = CreateWidget<UHealthBarWidget>(playerController, HealthWidgetWorldSpaceTemplate);
		healthWidget->HealthComponent = HealthComponent;
		HealthWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		HealthWidgetComponent->SetWidget(healthWidget);
	}
}

// Binds inputs.
void AUnrealTestCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);

	JumpBinding(PlayerInputComponent);

	MovementBinding(PlayerInputComponent);

	TurnBinding(PlayerInputComponent);
	LookUpBinding(PlayerInputComponent);

	ShootBinding(PlayerInputComponent);
}
#pragma endregion Overrides

#pragma region Functions
// Called for forwards/backward input.
void AUnrealTestCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

// Called for side to side input.
void AUnrealTestCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

// Called via input to turn at a given rate.
void AUnrealTestCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

// CCalled via input to turn look up/down at a given rate.
void AUnrealTestCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

// Start shooting.
void AUnrealTestCharacter::Shoot()
{
	Server_Shoot();
}

// Stop shooting.
void AUnrealTestCharacter::StopShoot() { }

// Server shoot handle validation.

bool AUnrealTestCharacter::Server_Shoot_Validate()
{
	return true;
}

// Server shoot handle.
void AUnrealTestCharacter::Server_Shoot_Implementation()
{
	CurrentWeapon->Shoot(FollowCamera);
}

// Call game over event validation.
bool AUnrealTestCharacter::Client_GameOver_Validate(int32 DefeatedTeamID)
{
	return true;
}

// Call game over event.
void AUnrealTestCharacter::Client_GameOver_Implementation(int32 DefeatedTeamID)
{
	// Show game over screen
	if (PlayerHUD) {
		PlayerHUD->UpdateDefeatedTeamID(true);
		PlayerHUD->UpdateGameOverWidgetVisibility(true); 
	};

	// Disable input
	GetController()->DisableInput(Cast<APlayerController>(GetController()));
}

// Set up character movement component.
void AUnrealTestCharacter::ConfigureCharacterMovement(UCharacterMovementComponent* characterMovement)
{
	// Configure character movement
	characterMovement->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	characterMovement->JumpZVelocity = JUMP_Z_VELOCITY;
	characterMovement->AirControl = AIR_CONTROL;
	characterMovement->MaxWalkSpeed = MAX_WALK_SPEED;
	characterMovement->MinAnalogWalkSpeed = MIN_ANALOG_WALK_SPEED;
	characterMovement->BrakingDecelerationWalking = BRAKING_DECELERATION_WALKING;
}

// Binds jump inputs.
void AUnrealTestCharacter::JumpBinding(class UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
}

// Binds movement inputs.
void AUnrealTestCharacter::MovementBinding(class UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AUnrealTestCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AUnrealTestCharacter::MoveRight);
}

// Binds shoot inputs.
void AUnrealTestCharacter::ShootBinding(class UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAction("Shoot", IE_Pressed, this, &AUnrealTestCharacter::Shoot);
	PlayerInputComponent->BindAction("Shoot", IE_Released, this, &AUnrealTestCharacter::StopShoot);
}

// Binds turning inputs.
void AUnrealTestCharacter::TurnBinding(class UInputComponent* PlayerInputComponent)
{
	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AUnrealTestCharacter::TurnAtRate);
}

// Binds aiming inputs.
void AUnrealTestCharacter::LookUpBinding(class UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AUnrealTestCharacter::LookUpAtRate);
}

// Respawn player.
void AUnrealTestCharacter::RespawnPlayer()
{
	HealthComponent->InitializeHealth(MaxHealth);
}

// Die event validation.
bool AUnrealTestCharacter::Multicast_Die_Validate() {
	return true;
}

// Die event.
void AUnrealTestCharacter::Multicast_Die_Implementation() {
	//Disables input after death
	if (IsLocallyControlled()) {
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		if (Cast<APlayerController>(GetController())) {
			DisableInput(PlayerController);
		}
	}

	//Hide and disable character
	SetActorHiddenInGame(true);
	GetCapsuleComponent()->SetEnableGravity(false);
	GetCharacterMovement()->GravityScale = 0.f;
	SetActorEnableCollision(false);
}
#pragma endregion Functions
