#include "TopDownPlayerController.h"

ATopDownPlayerController::ATopDownPlayerController()
{
	// Show the cursor so player can click on things in the world
	bShowMouseCursor = true;

	//Don`t lock the mouse to the viewport - lets RMB drag feel natural
	DefaultMouseCursor = EMouseCursor::Default;
}


