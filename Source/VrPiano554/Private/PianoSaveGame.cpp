#include "PianoSaveGame.h"

UPianoSaveGame::UPianoSaveGame()
{
	SaveSlotName = TEXT("PianoPositionSaveSlot");
	UserIndex = 0;
	PianoTransform = FTransform::Identity;
}
