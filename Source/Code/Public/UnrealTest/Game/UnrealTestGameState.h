// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Unreal Engine
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

// Game Project
#include "UnrealTestGameState.generated.h"

// Definitions
class AUnrealTestGameMode;
enum class EMatchPhase : uint8;

/**
 * 
 */
UCLASS()
class UNREALTEST_API AUnrealTestGameState : public AGameStateBase
{
	GENERATED_BODY()


#pragma region Delegates
// Delegates
public:
	// On match phase changed
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChanged, EMatchPhase, NewMatchPhase);
	UPROPERTY(BlueprintAssignable, Category = "SessionHandling")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

	// On player max count changed
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerMaxChanged, int32, MaxPlayers);
	UPROPERTY(BlueprintAssignable, Category = "SessionHandling")
	FOnPlayerMaxChanged OnPlayerMaxChanged;

	// On player current count changed
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerJoined, int32, CurrentPlayers);
	UPROPERTY(BlueprintAssignable, Category = "SessionHandling")
	FOnPlayerJoined OnPlayerJoined;

#pragma endregion Delegates

#pragma region Variables
// Variables
private:
	// Game mode reference
	AUnrealTestGameMode* GameMode;

	// Current players in session
	UPROPERTY(Replicated, Category = "SessionHandling")
	int32 CurrentPlayersInSession = 0;

	// Max players in session
	UPROPERTY(Replicated, Category = "SessionHandling")
	int32 MaxPlayersInSession = 0;

	// Match phase
	UPROPERTY(Replicated, Category = "GameMode")
	EMatchPhase GamePhase;

#pragma endregion Variables

#pragma region Initialization
	// Override

	// Constructor
	AUnrealTestGameState();
#pragma endregion Initialization

#pragma region Override
// Override

	// Replication settings
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#pragma endregion Override

#pragma region Getters / Setters
// Getters / Setters

public:
	// Get current player count in session
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "SessionHandling")
	int32 GetCurrentPlayersInSession() { return CurrentPlayersInSession; }

	// Get max player count in session
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "SessionHandling")
	int32 GetMaxPlayerInSession() { return MaxPlayersInSession; }

	// Get current match phase
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "GameMode")
	EMatchPhase GetMatchPhase() { return GamePhase; }
	
	// Set current player count in session
	UFUNCTION(BlueprintCallable, Category = "SessionHandling")
	void SetPlayerInSession(int32 NewPlayerCount);

	// Set max player count in session
	UFUNCTION(BlueprintCallable, Category = "SessionHandling")
	void SetMaxPlayerInSession(int32 NewMaxPlayerCount);

	// Set current match phase
	UFUNCTION(BlueprintCallable, Category = "GameMode")
	void SetMatchPhase(EMatchPhase NewPhase);

#pragma endregion Getters / Setters

};
