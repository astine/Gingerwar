#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include "SDL.h"

//Constants for defining the display
#define BPP      4
#define WIDTH  640
#define HEIGHT 480
#define DEPTH   32

//Constants for defining the game area 
#define TOP    14
#define BOTTOM  0
#define RIGHT  19
#define LEFT    0

//Constants defining game objects
#define TILE_WIDTH    32
#define TILE_HEIGHT   32
#define TILE_CENTER_X 15
#define TILE_CENTER_Y 15

//Directional signifiers
#define HORIZONTAL  1
#define VERTICAL    2

enum boolean {FALSE, TRUE};

enum ObjectType {PLAYER, MONSTER, PLATFORM};

//Stores a location
struct point 
{
  int x;
  int y;
};

//Stores a direction or speed
struct vector
{
  float x;
  float y;
};

//An image associated with a game object
struct icon 
{
  SDL_Surface *image;
  struct point center;
};

//Predefined images for each game object
struct icon player_icon = 
  { NULL, {15, 15} };
struct icon block_icon = 
  { NULL, {15, 15} };
struct icon monster_icon = 
  { NULL, {15, 15} };

/*
  Game objects: these are objects that interact in the game world
  They all have a location, speed, representational image and a 
  type. They are all affected by the game's collision detection.
*/
struct object 
{
  struct point location;
  struct point center;
  struct vector speed;
  struct icon *icon;
  enum boolean alive;
  enum ObjectType type;
} player =
  {
    {10, 0},
    {TILE_CENTER_X, TILE_CENTER_Y},
    {0, 0},
    &player_icon,
    TRUE,
    PLAYER
  };

/*
  Some constants that only apply to the player object
  These signify that the player was stopped by running
  into a wall and prevent accidental acceleration when
  keys are released.
*/
enum boolean blocked_left  = FALSE;
enum boolean blocked_right = FALSE;

//Objects in the game are generally stored and managed through 
//in linked lists
struct object_list
{
  struct object object;
  struct object_list *next_object;
};

struct object_list *g_blocks = NULL;
struct object_list *g_monsters = NULL;

/*
  The purpose of this grid is for collision detection. Each cell
  in the grid represents a square in the game area and contains a
  pointer to the game object (if any) that occupies that spot.
  This way, there is a simply O(1) method of determining whether any
  particular point on the map is occupied.
*/
struct object *grid[RIGHT+1][TOP+1] =
  { {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {&player, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL,    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL} };

//Some general collision detection functions. They do not
//detect what the colliding object is, only it's location.
enum boolean onFloor(struct object *obj)
{ return (obj->location.y == BOTTOM || (grid[obj->location.x][obj->location.y-1] != NULL)); }

enum boolean atCeiling(struct object *obj)
{ return (obj->location.y == TOP || (grid[obj->location.x][obj->location.y+1] != NULL)); }

enum boolean atRightWall(struct object *obj)
{ return (obj->location.x == RIGHT || (grid[obj->location.x+1][obj->location.y] != NULL)); }

enum boolean atLeftWall(struct object *obj)
{ return (obj->location.x == LEFT || (grid[obj->location.x-1][obj->location.y] != NULL)); }

enum boolean atCorner(struct object *obj, struct vector direction)
{
  int x = (int)(direction.x / fabs(direction.x));
  int y = (int)(direction.y / fabs(direction.y));
  return (grid[obj->location.x+x][obj->location.y+y] != NULL);
}


//Stops an object and centers it in its square.
void StopObject(struct object *obj, int direction)
{
  switch (direction)
    {
    case HORIZONTAL:
      obj->speed.x = 0;
      obj->center.x = TILE_CENTER_X;
      break;
    case VERTICAL:
      obj->speed.y = 0;
      obj->center.y = TILE_CENTER_Y;
      break;
    case HORIZONTAL|VERTICAL:
      obj->speed.x = 0;
      obj->center.x = TILE_CENTER_X;
      obj->speed.y = 0;
      obj->center.y = TILE_CENTER_Y;
      break;
    }
}

/*
  Moves and object in 'direction,' updating the grid as it does so
  to reflect the move. Objects move within their squares before they
  move between them.
*/
void MoveObject(struct object *object, struct vector *direction)
{
  int new_center_x = object->center.x + round(direction->x * TILE_WIDTH);
  int new_center_y = object->center.y + round(direction->y * TILE_HEIGHT);
  
  grid[object->location.x][object->location.y] = NULL;
  
  if (new_center_x < 0)
    {
      object->center.x = new_center_x + TILE_WIDTH;
      object->location.x--;
    }
  else if (new_center_x > TILE_WIDTH)
    {
      object->center.x = new_center_x - TILE_WIDTH;
      object->location.x++;
    }
  else
    { object->center.x = new_center_x; }
  
  if (new_center_y < 0)
    {
      object->center.y = new_center_y + TILE_HEIGHT;
      object->location.y--;
    }
  else if (new_center_y > TILE_HEIGHT)
    {
      object->center.y = new_center_y - TILE_HEIGHT;
      object->location.y++;
    }
  else
    { object->center.y = new_center_y; }
  
  grid[object->location.x][object->location.y] = object;
}

/********************************************************************\
                              Graphics
\********************************************************************/

//Sets the pixel signified by 'x' and 'y' to 'color.'
void DrawPixel(SDL_Surface *screen, int x, int y, Uint32 color)
{
  Uint32 *bufp;
    
  bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
  *bufp = color;
}

//Sets the entire screen to 'color.'
void ClearScreen(SDL_Surface *screen, Uint32 color)
{
  Uint32 *bufp;
  bufp = (Uint32 *)screen->pixels;
  
  for(int i = 0; i < screen->h * screen->w; i++ ) 
    {
      *bufp = color;
      bufp++;
    }
}

//Blits an icon to the screen location set by 'x' and 'y,' 
//offset by the icon center.
void DrawIcon(SDL_Surface *screen, struct icon *icon, int x, int y)
{
  SDL_Rect dest;
  dest.x = x - icon->center.x;
  dest.y = y - icon->center.y;
  dest.w = icon->image->w;
  dest.h = icon->image->h;
  
  if(icon->image == NULL)
    { printf("Bad Image\n"); }
  else 
    { SDL_BlitSurface(icon->image, NULL, screen, &dest);}
}

//Blits object icon to the objects square and offset "center"
//in the game area.
void DrawObject(SDL_Surface *screen, struct object *obj)
{
  SDL_Rect dest;
  dest.x = obj->location.x * TILE_WIDTH;
  dest.y = (14 - obj->location.y) * TILE_HEIGHT;
  dest.w = TILE_WIDTH - 1;
  dest.h = TILE_HEIGHT - 1;

  DrawIcon(screen, obj->icon, dest.x + obj->center.x,
	   dest.y + (TILE_HEIGHT - (1 + obj->center.y)));
}

//Draws the player to the screen
void DrawTortoise(SDL_Surface *screen)
{
  DrawObject(screen, &player);
  
  Uint32 color = SDL_MapRGB(screen->format, 255,255,255);

  int left = player.location.x * TILE_WIDTH;
  int right = left + 31;
  int top = (14 - player.location.y) * TILE_HEIGHT;
  int bottom = top + 31;

  DrawPixel(screen, left, top, color);
  DrawPixel(screen, left, bottom, color);
  DrawPixel(screen, right, top, color);
  DrawPixel(screen, right, bottom, color);
}

/*********************************************************\
                         Environment
\*********************************************************/

//Returns a pointer to the last object in the list
struct object_list *FindEndOfObjects(struct object_list *objects)
{
  return (objects->next_object == NULL) ? objects : FindEndOfObjects(objects->next_object);
}

//Inserts a new object to the end of the list.
//Also adds a reference in grid to new object.
void CreateObject(struct object_list **objects, struct point location, struct point center, struct vector speed, struct icon *icon, enum ObjectType object_type)
{
  struct object_list *new_object;
  new_object = malloc(sizeof(struct object_list));
  
  new_object->object.location = location;
  new_object->object.center = center;
  new_object->object.speed = speed;
  new_object->object.icon = icon;
  new_object->object.alive = TRUE;
  new_object->object.type = object_type;
  new_object->next_object = NULL;  
  
  grid[location.x][location.y] = &(new_object->object);
  
  if (*objects == NULL)
    {
      *objects = new_object;
    }
  else
    { 
      struct object_list *last_object = FindEndOfObjects(*objects);
      last_object->next_object = new_object;
    }
}

//Removes object from list, but not from the grid.
//This is to accommodate objects that shouldn't be collision
//detected, but still need to be printed. (only dead objects)
void DestroyObject(struct object_list **object)
{
  struct object_list *tail = (**object).next_object;
  free(*object);
  *object = tail;
}

/*********************************************************\
                           Main Loop 
\*********************************************************/

void initialize()
{
  //Initialize display
  if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 )
    {
      fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
      exit(1);
    }
  atexit(SDL_Quit);
  
  //Preload icon images
  player.icon->image = SDL_LoadBMP("gingerbread.bmp");
  if ( player.icon->image == NULL )
    {
      fprintf(stderr, "Couldn't load %s: %s\n", "gingerbread.bmp", SDL_GetError());
      return;
    }
  block_icon.image = SDL_LoadBMP("block.bmp");
  if ( block_icon.image == NULL )
    {
      fprintf(stderr, "Couldn't load %s: %s\n", "block.bmp", SDL_GetError());
      return;
    }
  monster_icon.image = SDL_LoadBMP("monster.bmp");
  if ( block_icon.image == NULL )
    {
      fprintf(stderr, "Couldn't load %s: %s\n", "monster.bmp", SDL_GetError());
      return;
    }
  
  //Generate platform objects and place them according to the
  //schema established in the 'map.txt' file.
  struct point center = {TILE_CENTER_X, TILE_CENTER_Y};
  struct vector speed = {0, 0};
  FILE *fp = fopen("map.txt","r");
  for(int y = TOP; y > BOTTOM; y--)
    {
      for(int x = LEFT; x <= RIGHT + 1; x++)
	{
	  if (getc(fp) == '*')
	    {
	      struct point location;
	      location.x = x;
	      location.y = y;
	      CreateObject(&g_blocks, location, center, speed, &block_icon, PLATFORM );
	    }
	}
    }
  
  //Create initial monsters
  struct point location = {LEFT,TOP};
  CreateObject(&g_monsters, location, center, speed, &monster_icon, MONSTER);
  location.x = RIGHT;
  CreateObject(&g_monsters, location, center, speed, &monster_icon, MONSTER);
}

/*
  UpdateMonsters simulates the behavior of all of the game's "monster"
  objects. The "physics" and "AI" associated with them happen here.
 */
void UpdateMonsters()
{
  for(struct object_list *monsterp = g_monsters; monsterp != NULL; monsterp = monsterp->next_object)
    {
      //If monster happens to be dead, merely cause it to fall some.
      if(!monsterp->object.alive)
	{
	  if(monsterp->object.location.y > 0)
	    { monsterp->object.location.y--; }
	  break;
	}
      
      //Kill monsters that reach the end of their paths (bottom two corners),
      //More are constantly spawned anyway.
      if(monsterp->object.location.y == BOTTOM &&
	 (monsterp->object.location.x == RIGHT ||
	  monsterp->object.location.x == LEFT))
	{
	  monsterp->object.alive = FALSE;
	  break;
	}

      //When a monsters hits an obstacle, have it reverse direction. (Also, start
      //moving if it's sitting next to a wall.
      if(monsterp->object.speed.x >= 0 && atRightWall(&(monsterp->object)))
	{ monsterp->object.speed.x = -0.15; }
      else if(monsterp->object.speed.x <= 0 && atLeftWall(&(monsterp->object)))
	{ monsterp->object.speed.x = 0.15; }
       
      //Make sure monsters don't go through ceilings
      if(monsterp->object.speed.y >= 0 && atCeiling(&monsterp->object))
	{ StopObject(&monsterp->object,VERTICAL); }
      //Give monsters gravity
      if(monsterp->object.speed.y <= 0 && onFloor(&monsterp->object))
	{ StopObject(&monsterp->object,VERTICAL); }
      else if(monsterp->object.speed.y > -0.5)
	{ monsterp->object.speed.y -= 0.05; }
      else if(monsterp->object.speed.y == 0)
	{ monsterp->object.center.y = TILE_CENTER_Y; }
      
      //Make sure monsters don't go diagonally through corners
      if(atCorner(&monsterp->object,monsterp->object.speed) &&
	 !(monsterp->object.speed.x == 0 && monsterp->object.speed.y == 0))
	{
	  if(abs(monsterp->object.speed.x) > abs(monsterp->object.speed.y))
	    { StopObject(&monsterp->object,HORIZONTAL); }
	  else
	    { StopObject(&monsterp->object,VERTICAL); }
	}
    }

  //Remove any monsters that happen to be dead and have fallen to the bottom of the
  //game area.
  struct object_list **next;
  for(struct object_list **monsterp = &g_monsters; *monsterp != NULL; monsterp = next)
    {
      next = &((*monsterp)->next_object);
      if(!(**monsterp).object.alive && (**monsterp).object.location.y <= BOTTOM)
	{ DestroyObject(monsterp); }
    }
}

void UpdateState()
{
  //If player is still, center it
  if(player.speed.x == 0)
    { player.center.x = TILE_CENTER_X; }
  
  //Stop player if he hits a wall
  if((player.speed.x >= 0 && atRightWall(&player)) ||
     (player.speed.x <= 0 && atLeftWall(&player)))
    {
      if(player.speed.x < 0)
	{ blocked_right = TRUE; }
      else if(player.speed.x > 0)
	{ blocked_left = TRUE; }
      StopObject(&player,HORIZONTAL);
    }
  
  //Keep player from going through ceilings
  if(player.speed.y >= 0 && atCeiling(&player))
    { StopObject(&player,VERTICAL); }
  //Create gravity for player
  if(player.speed.y <= 0 && onFloor(&player))
    { StopObject(&player,VERTICAL); }
  else if(player.speed.y > -0.5)
    { player.speed.y -= 0.05; }
  else if(player.speed.y == 0)
    { player.center.y = TILE_CENTER_Y; }
  
  //Make sure player doesn't go diagonally through corners
  if(atCorner(&player,player.speed) && !(player.speed.x == 0 && player.speed.y == 0))
    {
      if(abs(player.speed.x) > abs(player.speed.y))
	{ StopObject(&player,HORIZONTAL); }
      else
	{ StopObject(&player,VERTICAL); }
    }
  
  //Detect collisions between player and monsters:
  //Kill the monster if the player lands on it, but kill the player
  //otherwise.
  if(grid[player.location.x][player.location.y-1] != NULL &&
     grid[player.location.x][player.location.y-1]->type == MONSTER)
    {
      grid[player.location.x][player.location.y-1]->alive = FALSE;
      grid[player.location.x][player.location.y-1] = NULL;
    }
  else if((player.location.y != TOP &&
	   grid[player.location.x][player.location.y+1] != NULL &&
	   grid[player.location.x][player.location.y+1]->type == MONSTER) ||
	  (player.location.x != LEFT &&
	   grid[player.location.x-1][player.location.y] != NULL &&
	   grid[player.location.x-1][player.location.y]->type == MONSTER) ||
	  (player.location.x != RIGHT &&
	   grid[player.location.x+1][player.location.y] != NULL &&
	   grid[player.location.x+1][player.location.y]->type == MONSTER))
    { player.alive = FALSE; }
  
  //Every second, spawn a new monster in one of the top two corners
  static time_t last_time = 0;
  time_t now = time(NULL);
  if (difftime(now, last_time) > 1)
    {
      struct point center = {TILE_CENTER_X, TILE_CENTER_Y};
      struct vector speed = {0, 0};
      struct point location = {LEFT,TOP};
      if(rand()%10 >= 5)
	{ location.x = RIGHT; }
      CreateObject(&g_monsters, location, center, speed, &monster_icon, MONSTER);
      last_time = time(NULL);
    }
  
  //update the monsters
  UpdateMonsters();
  
  //change the player's location
  MoveObject(&player, &player.speed);
  
  //change the monsters' locations
  for(struct object_list *monsterp = g_monsters; monsterp != NULL; monsterp = monsterp->next_object)
    {
      if(monsterp->object.alive)
	{ MoveObject(&(monsterp->object), &(monsterp->object.speed)); }
    }
}

//Render the game state
void RenderState(SDL_Surface *screen)
{   
  if ( SDL_MUSTLOCK(screen) )
    {
      if ( SDL_LockSurface(screen) < 0 )
	{ return; }
    }
  
  //Clear the screen to the background color (black)
  ClearScreen(screen, SDL_MapRGB(screen->format, 0,0,0));
  
  //Draw the platforms
  for(struct object_list *blockp = g_blocks; blockp != NULL; blockp = blockp->next_object)
    { DrawObject(screen, &(blockp->object)); }
  
  //Draw the monsters 
  for(struct object_list *monsterp = g_monsters; monsterp != NULL; monsterp = monsterp->next_object)
    { DrawObject(screen, &(monsterp->object)); }
  
  //Draw the player 
  DrawTortoise(screen);
  
  if ( SDL_MUSTLOCK(screen) )
    { SDL_UnlockSurface(screen); }
  
  //Update screen for player to see
  SDL_UpdateRect(screen, 0, 0, WIDTH, HEIGHT);
}

//At end game, render either a "victory" screen or a "loss" screen
void RenderFinal(SDL_Surface *screen, enum boolean victory)
{
  if ( SDL_MUSTLOCK(screen) )
    {
      if ( SDL_LockSurface(screen) < 0 )
	{ return; }
    }
  
  ClearScreen(screen, SDL_MapRGB(screen->format, 0,0,0));
      
  SDL_Surface *image;
  SDL_Rect dest = { 0, 0, WIDTH, HEIGHT };
  
  if(victory)
    { image = SDL_LoadBMP("victory.bmp"); }
  else
    { image = SDL_LoadBMP("loss.bmp"); }
  
  if ( image == NULL )
    {
      fprintf(stderr, "Couldn't load %s: %s\n", "victory.bmp", SDL_GetError());
      return;
    }
      
  SDL_BlitSurface(image, NULL, screen, &dest);
      
  if ( SDL_MUSTLOCK(screen) )
    { SDL_UnlockSurface(screen); }
  
  SDL_UpdateRect(screen, 0, 0, WIDTH, HEIGHT);
  
  //Sleep a moment so player doesn't accidentally exit before he's had the
  //opportunity to realize the game is over.
  sleep(1);
  
  //Wait for player input to exit.
  SDL_Event event;
  SDL_WaitEvent(&event);
  exit(0);
}

//Handle user input
void HandleEvents()
{
  SDL_Event event;

  while(SDL_PollEvent(&event))
    {
      switch (event.type)
	{
	case SDL_KEYDOWN:
	  switch (event.key.keysym.sym)
	    {
	      //Left and Right keys cause the user to accelerate respectively
	      //Up key jumps.
	    case SDLK_UP:
	      if(onFloor(&player))
		{ player.speed.y = 0.7; }
	      break;
	    case SDLK_DOWN:
	      if(onFloor(&player))
		{ player.speed.y = -0.7; }
	      break;
	    case SDLK_RIGHT:
	      player.speed.x += 0.25;
	      break;
	    case SDLK_LEFT:
	      player.speed.x -= 0.25;
	      break;
	    case SDLK_UNKNOWN:
	      break;
	    default:
	      break;
	    }
	  break;
	case SDL_KEYUP:
	  switch (event.key.keysym.sym)
	    {
	      //Releasing the left and right keys cause the player to accelerate
	      //in the opposite direction (to counteract the initial acceleration)
	      //unless the player has already been stopped by a wall.
	    case SDLK_UP:
	      break;
	    case SDLK_DOWN:
	      break;
	    case SDLK_RIGHT:
	      if(blocked_left)
		{ blocked_left= FALSE; }
	      else
		{ player.speed.x -= 0.25; }
	      break;
	    case SDLK_LEFT:
	      if(blocked_right)
		{ blocked_right= FALSE; }
	      else
		{ player.speed.x += 0.25; }
	      break;
	    case SDLK_UNKNOWN:
	      break;
	    default:
	      break;
	    }
	  break;
	case SDL_QUIT:
	  exit(0);
	}
    }
}

int main(int argc, char *argv[])
{
  initialize();
  
  srand(time(NULL));
    
  SDL_Surface *screen;
  screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_SWSURFACE);
  
  if ( screen == NULL )
    {
      fprintf(stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
      exit(1);
    }
  
  //Main game loop
  while(1)
    {
      //Check for end game conditions
      if (!player.alive)
	{ RenderFinal(screen, FALSE); }
      else if (g_monsters == NULL)
	{ RenderFinal(screen, TRUE); }
	
      UpdateState();

      RenderState(screen);
      
      HandleEvents();
      
      usleep(50000);
    }
}

