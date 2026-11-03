#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EyeDiseasesSimulatorGameMode.generated.h"

/**
 * Bazowy GameMode szablonu MR. Blueprint BP_MRGameMode nadpisuje Default Pawn Class na BP_MRPawn.
 */
UCLASS()
class EYEDISEASESSIMULATOR_API AEyeDiseasesSimulatorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEyeDiseasesSimulatorGameMode();
};
