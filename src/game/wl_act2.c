#include "def.h"
// WL_ACT2.C

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define PROJECTILESIZE	0xc000l

#define BJRUNSPEED	2048
#define BJJUMPSPEED	680


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/



/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/


/* angle (0..ANGLES-1, counter clockwise, y up) of the vector (dx,dy) */
extern const u16 tan_q[46];             /* tan(a degrees) * 65536, a = 0..45 */
s16 AimAngle (s32 dy, s32 dx) B
{
	u32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
	u16 ratio;
	s16 a, lo, hi;
	u8 swap = 0;

	if (!ax && !ay)
		return 0;
	while (ax >= 32768 || ay >= 32768)
	{
		ax >>= 1;
		ay >>= 1;
	}
	if (ay > ax)
	{
		u32 t = ax; ax = ay; ay = t;
		swap = 1;
	}
	ratio = (ay == ax) ? 65535 : udiv32_16(ay << 16, (u16)ax);
	lo = 0; hi = 45;
	while (lo < hi)
	{
		s16 mid = (lo + hi) >> 1;
		if (tan_q[mid] < ratio) lo = mid + 1; else hi = mid;
	}
	a = lo;
	if (a > 0 && (u16)(ratio - tan_q[a - 1]) < (u16)(tan_q[a] - ratio))
		a--;
	if (swap) a = 90 - a;
	if (dx < 0) a = 180 - a;
	if (dy < 0) a = 360 - a;
	if (a >= 360) a -= 360;
	return a;
}

dirtype dirtable[9] = {northwest,north,northeast,west,nodir,east,
	southwest,south,southeast};

const s16 starthitpoints[4][NUMENEMIES] =
	 //
	 // BABY MODE
	 //
	 {
	 {25,	// guards
	  50,	// officer
	  100,	// SS
	  1,	// dogs
	  850,	// Hans
	  850,	// Schabbs
	  200,	// fake hitler
	  800,	// mecha hitler
	  45,	// mutants
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts

	  850,	// Gretel
	  850,	// Gift
	  850,	// Fat
	  5,	// en_spectre,
	  1450,	// en_angel,
	  850,	// en_trans,
	  1050,	// en_uber,
	  950,	// en_will,
	  1250	// en_death
	  },
	 //
	 // DON'T HURT ME MODE
	 //
	 {25,	// guards
	  50,	// officer
	  100,	// SS
	  1,	// dogs
	  950,	// Hans
	  950,	// Schabbs
	  300,	// fake hitler
	  950,	// mecha hitler
	  55,	// mutants
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts

	  950,	// Gretel
	  950,	// Gift
	  950,	// Fat
	  10,	// en_spectre,
	  1550,	// en_angel,
	  950,	// en_trans,
	  1150,	// en_uber,
	  1050,	// en_will,
	  1350	// en_death
	  },
	 //
	 // BRING 'EM ON MODE
	 //
	 {25,	// guards
	  50,	// officer
	  100,	// SS
	  1,	// dogs

	  1050,	// Hans
	  1550,	// Schabbs
	  400,	// fake hitler
	  1050,	// mecha hitler

	  55,	// mutants
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts

	  1050,	// Gretel
	  1050,	// Gift
	  1050,	// Fat
	  15,	// en_spectre,
	  1650,	// en_angel,
	  1050,	// en_trans,
	  1250,	// en_uber,
	  1150,	// en_will,
	  1450	// en_death
	  },
	 //
	 // DEATH INCARNATE MODE
	 //
	 {25,	// guards
	  50,	// officer
	  100,	// SS
	  1,	// dogs

	  1200,	// Hans
	  2400,	// Schabbs
	  500,	// fake hitler
	  1200,	// mecha hitler

	  65,	// mutants
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts
	  25,	// ghosts

	  1200,	// Gretel
	  1200,	// Gift
	  1200,	// Fat
	  25,	// en_spectre,
	  2000,	// en_angel,
	  1200,	// en_trans,
	  1400,	// en_uber,
	  1300,	// en_will,
	  1600	// en_death
	  }}
	  ;

void	A_StartDeathCam (objtype *ob) B;


void	T_Path (objtype *ob) B;
void	T_Shoot (objtype *ob) B;
void	T_Bite (objtype *ob) B;
void	T_DogChase (objtype *ob) B;
void	T_Chase (objtype *ob) B;
void	T_Projectile (objtype *ob) B;
void	T_Stand (objtype *ob) B;

void A_DeathScream (objtype *ob) B;

void A_Smoke (objtype *ob) B;










void	T_Schabb (objtype *ob) B;
void	T_SchabbThrow (objtype *ob) B;
void	T_Fake (objtype *ob) B;
void	T_FakeFire (objtype *ob) B;
void	T_Ghosts (objtype *ob) B;

void A_Slurpie (objtype *ob) B;
void A_HitlerMorph (objtype *ob) B;
void A_MechaSound (objtype *ob) B;

/*
=================
=
= A_Smoke
=
=================
*/




/*
===================
=
= ProjectileTryMove
=
= returns TRUE if move ok
===================
*/

#define PROJSIZE	0x2000





/*
=================
=
= T_Projectile
=
=================
*/






/*
=============================================================================

							GUARD

=============================================================================
*/

//
// guards
//
























//
// ghosts
//









//
// dogs
//























//
// officers
//

























//
// mutant
//


























//
// SS
//






























//
// hans
//





















//
// gretel
//





















/*
===============
=
= SpawnStand
=
===============
*/

void SpawnStand (enemy_t which, s16 tilex, s16 tiley, s16 dir) B
{
	switch (which)
	{
	case en_guard:
		SpawnNewObj (tilex,tiley,&s_grdstand);
		newobj->speed = SPDPATROL;
		if (!loadedgame)
		  gamestate.killtotal++;
		break;

	case en_ss:
		SpawnNewObj (tilex,tiley,&s_ssstand);
		newobj->speed = SPDPATROL;
		if (!loadedgame)
		  gamestate.killtotal++;
		break;
	}


	if (AreaOf(tilex, tiley) & 0x40)
		newobj->flags |= FL_AMBUSH;

	newobj->obclass = guardobj+which;
	newobj->hitpoints = starthitpoints[gamestate.difficulty][which];
	newobj->dir = dir*2;
	newobj->flags |= FL_SHOOTABLE;
}



/*
===============
=
= SpawnDeadGuard
=
===============
*/

void SpawnDeadGuard (s16 tilex, s16 tiley) B
{
	SpawnNewObj (tilex,tiley,&s_grddie4);
	newobj->obclass = inertobj;
}



/*
===============
=
= SpawnBoss
=
===============
*/

void SpawnBoss (s16 tilex, s16 tiley) B
{
	u8 *map;
	u16 tile;

	SpawnNewObj (tilex,tiley,&s_bossstand);
	newobj->speed = SPDPATROL;

	newobj->obclass = bossobj;
	newobj->hitpoints = starthitpoints[gamestate.difficulty][en_boss];
	newobj->dir = south;
	newobj->flags |= FL_SHOOTABLE|FL_AMBUSH;
	if (!loadedgame)
	  gamestate.killtotal++;
}

/*
===============
=
= SpawnGretel
=
===============
*/



/*
===============
=
= SpawnPatrol
=
===============
*/

void SpawnPatrol (enemy_t which, s16 tilex, s16 tiley, s16 dir) B
{
	switch (which)
	{
	case en_guard:
		SpawnNewObj (tilex,tiley,&s_grdpath1);
		newobj->speed = SPDPATROL;
		if (!loadedgame)
		  gamestate.killtotal++;
		break;

	case en_ss:
		SpawnNewObj (tilex,tiley,&s_sspath1);
		newobj->speed = SPDPATROL;
		if (!loadedgame)
		  gamestate.killtotal++;
		break;

	case en_dog:
		SpawnNewObj (tilex,tiley,&s_dogpath1);
		newobj->speed = SPDDOG;
		if (!loadedgame)
		  gamestate.killtotal++;
		break;
	}

	newobj->obclass = guardobj+which;
	newobj->dir = dir*2;
	newobj->hitpoints = starthitpoints[gamestate.difficulty][which];
	newobj->distance = TILEGLOBAL;
	newobj->flags |= FL_SHOOTABLE;
	newobj->active = TRUE;

	actorat[newobj->tilex][newobj->tiley] = 0;		// don't use original spot

	switch (dir)
	{
	case 0:
		newobj->tilex++;
		break;
	case 1:
		newobj->tiley--;
		break;
	case 2:
		newobj->tilex--;
		break;
	case 3:
		newobj->tiley++;
		break;
	}

	actorat[newobj->tilex][newobj->tiley] = act_enc(newobj);
}



/*
==================
=
= A_DeathScream
=
==================
*/

void A_DeathScream (objtype *ob) B
{

	switch (ob->obclass)
	{
	case guardobj:
		{
		 s16 sounds[9]={ DEATHSCREAM1SND,
				 DEATHSCREAM2SND,
				 DEATHSCREAM3SND,
				 DEATHSCREAM4SND,
				 DEATHSCREAM5SND,
				 DEATHSCREAM7SND,
				 DEATHSCREAM8SND,
				 DEATHSCREAM9SND
				 };

		 PlaySoundLocActor(sounds[US_RndT()%2],ob);
		}
		break;
	case ssobj:
		PlaySoundLocActor(LEBENSND,ob);	// JAB
		break;
	case dogobj:
		PlaySoundLocActor(DOGDEATHSND,ob);	// JAB
		break;
	case bossobj:
		SD_PlaySound(MUTTISND);				// JAB
		break;
	}
}


/*
=============================================================================

						 SPEAR ACTORS

=============================================================================
*/


/*
=============================================================================

						 SCHABBS / GIFT / FAT

=============================================================================
*/

/*
===============
=
= SpawnGhosts
=
===============
*/





void T_Stand (objtype *ob) B
{
	SightPlayer (ob);
}


/*
============================================================================

								CHASE

============================================================================
*/

/*
=================
=
= T_Chase
=
=================
*/

void T_Chase (objtype *ob) B
{
	s32 move;
	s16	dx,dy,dist,chance;
	boolean	dodge;

	if (gamestate.victoryflag)
		return;

	dodge = FALSE;
	if (CheckLine(ob))	// got a shot at player?
	{
		dx = abs(ob->tilex - player->tilex);
		dy = abs(ob->tiley - player->tiley);
		dist = dx>dy ? dx : dy;
		if (!dist || (dist==1 && ob->distance<0x4000) )
			chance = 300;
		else
			chance = (tics<<4)/dist;

		if ( US_RndT()<chance)
		{
		//
		// go into attack frame
		//
			switch (ob->obclass)
			{
			case guardobj:
				NewState (ob,&s_grdshoot1);
				break;
			case ssobj:
				NewState (ob,&s_ssshoot1);
				break;
			case bossobj:
				NewState (ob,&s_bossshoot1);
				break;
			}
			return;
		}
		dodge = TRUE;
	}

	if (ob->dir == nodir)
	{
		if (dodge)
			SelectDodgeDir (ob);
		else
			SelectChaseDir (ob);
		if (ob->dir == nodir)
			return;							// object is blocked in
	}

	move = mul_tics ((u8)tics, (u16)ob->speed);

	while (move)
	{
		if (ob->distance < 0)
		{
		//
		// waiting for a door to open
		//
			OpenDoor (-ob->distance-1);
			if (doorobjlist[-ob->distance-1].action != dr_open)
				return;
			ob->distance = TILEGLOBAL;	// go ahead, the door is now opoen
		}

		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}

		//
		// reached goal tile, so select another one
		//

		//
		// fix position to account for round off during moving
		//
		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;

		move -= ob->distance;

		if (dodge)
			SelectDodgeDir (ob);
		else
			SelectChaseDir (ob);

		if (ob->dir == nodir)
			return;							// object is blocked in
	}

}


/*
=================
=
= T_Ghosts
=
=================
*/



/*
=================
=
= T_DogChase
=
=================
*/

void T_DogChase (objtype *ob) B
{
	s32 	move;
	s16		dist,chance;
	s32	dx,dy;


	if (ob->dir == nodir)
	{
		SelectDodgeDir (ob);
		if (ob->dir == nodir)
			return;							// object is blocked in
	}

	move = mul_tics ((u8)tics, (u16)ob->speed);

	while (move)
	{
	//
	// check for byte range
	//
		dx = player->x - ob->x;
		if (dx<0)
			dx = -dx;
		dx -= move;
		if (dx <= MINACTORDIST)
		{
			dy = player->y - ob->y;
			if (dy<0)
				dy = -dy;
			dy -= move;
			if (dy <= MINACTORDIST)
			{
				NewState (ob,&s_dogjump1);
				return;
			}
		}

		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}

		//
		// reached goal tile, so select another one
		//

		//
		// fix position to account for round off during moving
		//
		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;

		move -= ob->distance;

		SelectDodgeDir (ob);

		if (ob->dir == nodir)
			return;							// object is blocked in
	}

}



/*
============================================================================

								PATH

============================================================================
*/


/*
===============
=
= SelectPathDir
=
===============
*/

void SelectPathDir (objtype *ob) B
{
	u16 spot;

	spot = InfoPlane(ob->tilex,ob->tiley)-ICONARROWS;

	if (spot<8)
	{
	// newobj direction
		ob->dir = spot;
	}

	ob->distance = TILEGLOBAL;

	if (!TryWalk (ob))
		ob->dir = nodir;
}


/*
===============
=
= T_Path
=
===============
*/

void T_Path (objtype *ob) B
{
	s32 	move;
	s32 	deltax,deltay,size;

	if (SightPlayer (ob))
		return;

	if (ob->dir == nodir)
	{
		SelectPathDir (ob);
		if (ob->dir == nodir)
			return;					// all movement is blocked
	}


	move = mul_tics ((u8)tics, (u16)ob->speed);

	while (move)
	{
		if (ob->distance < 0)
		{
		//
		// waiting for a door to open
		//
			OpenDoor (-ob->distance-1);
			if (doorobjlist[-ob->distance-1].action != dr_open)
				return;
			ob->distance = TILEGLOBAL;	// go ahead, the door is now opoen
		}

		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}

		if (ob->tilex>MAPSIZE || ob->tiley>MAPSIZE)
		{
			Quit ("T_Path hit a wall");
		}



		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;
		move -= ob->distance;

		SelectPathDir (ob);

		if (ob->dir == nodir)
			return;					// all movement is blocked
	}
}


/*
=============================================================================

								FIGHT

=============================================================================
*/


/*
===============
=
= T_Shoot
=
= Try to damage the player, based on skill level and player's speed
=
===============
*/

void T_Shoot (objtype *ob) B
{
	s16	dx,dy,dist;
	s16	hitchance,damage;

	hitchance = 128;

	if (!areabyplayer[ob->areanumber])
		return;

	if (!CheckLine (ob))			// player is behind a wall
	  return;

	dx = abs(ob->tilex - player->tilex);
	dy = abs(ob->tiley - player->tiley);
	dist = dx>dy ? dx:dy;

	if (ob->obclass == ssobj || ob->obclass == bossobj)
		dist = dist*2/3;					// ss are better shots

	if (thrustspeed >= RUNSPEED)
	{
		if (ob->flags&FL_VISABLE)
			hitchance = 160-dist*16;		// player can see to dodge
		else
			hitchance = 160-dist*8;
	}
	else
	{
		if (ob->flags&FL_VISABLE)
			hitchance = 256-dist*16;		// player can see to dodge
		else
			hitchance = 256-dist*8;
	}

// see if the shot was a hit

	if (US_RndT()<hitchance)
	{
		if (dist<2)
			damage = US_RndT()>>2;
		else if (dist<4)
			damage = US_RndT()>>3;
		else
			damage = US_RndT()>>4;

		TakeDamage (damage,ob);
	}

	switch(ob->obclass)
	{
	 case ssobj:
	   PlaySoundLocActor(SSFIRESND,ob);
	   break;
	 case bossobj:
	   PlaySoundLocActor(BOSSFIRESND,ob);
	   break;
	 default:
	   PlaySoundLocActor(NAZIFIRESND,ob);
	}

}


/*
===============
=
= T_Bite
=
===============
*/

void T_Bite (objtype *ob) B
{
	s32	dx,dy;
	s16	hitchance,damage;


	PlaySoundLocActor(DOGATTACKSND,ob);	// JAB

	dx = player->x - ob->x;
	if (dx<0)
		dx = -dx;
	dx -= TILEGLOBAL;
	if (dx <= MINACTORDIST)
	{
		dy = player->y - ob->y;
		if (dy<0)
			dy = -dy;
		dy -= TILEGLOBAL;
		if (dy <= MINACTORDIST)
		{
		   if (US_RndT()<180)
		   {
			   TakeDamage (US_RndT()>>4,ob);
			   return;
		   }
		}
	}

	return;
}


/*
============================================================================

							BJ VICTORY

============================================================================
*/


//
// BJ victory
//

void T_BJRun (objtype *ob) B;
void T_BJJump (objtype *ob) B;
void T_BJDone (objtype *ob) B;
void T_BJYell (objtype *ob) B;

void T_DeathCam (objtype *ob) B;













/*
===============
=
= SpawnBJVictory
=
===============
*/

void SpawnBJVictory (void) B
{
	u8 *map;
	u16 tile;

	SpawnNewObj (player->tilex,player->tiley+1,&s_bjrun1);
	newobj->x = player->x;
	newobj->y = player->y;
	newobj->obclass = bjobj;
	newobj->dir = north;
	newobj->temp1 = 6;			// tiles to run forward
}



/*
===============
=
= T_BJRun
=
===============
*/

void T_BJRun (objtype *ob) B
{
	s32 	move;

	move = BJRUNSPEED*tics;

	while (move)
	{
		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}


		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;
		move -= ob->distance;

		SelectPathDir (ob);

		if ( !(--ob->temp1) )
		{
			NewState (ob,&s_bjjump1);
			return;
		}
	}
}


/*
===============
=
= T_BJJump
=
===============
*/

void T_BJJump (objtype *ob) B
{
	s32 	move;

	move = BJJUMPSPEED*tics;
	MoveObj (ob,move);
}


/*
===============
=
= T_BJYell
=
===============
*/

void T_BJYell (objtype *ob) B
{
	PlaySoundLocActor(YEAHSND,ob);	// JAB
}


/*
===============
=
= T_BJDone
=
===============
*/


void T_BJDone (objtype *ob) B
{
	playstate = ex_victorious;				// exit castle tile
}



//===========================================================================


/*
===============
=
= CheckPosition
=
===============
*/

boolean	CheckPosition (objtype *ob) B
{
	s16	x,y,xl,yl,xh,yh;
	objtype *check;

	xl = (ob->x-PLAYERSIZE) >>TILESHIFT;
	yl = (ob->y-PLAYERSIZE) >>TILESHIFT;

	xh = (ob->x+PLAYERSIZE) >>TILESHIFT;
	yh = (ob->y+PLAYERSIZE) >>TILESHIFT;

	//
	// check for solid walls
	//
	for (y=yl;y<=yh;y++)
		for (x=xl;x<=xh;x++)
		{
			u8 a = actorat[x][y];
			if (a && !ACT_ISACTOR(a))
				return FALSE;
		}

	return TRUE;
}


/*
===============
=
= A_StartDeathCam
=
===============
*/

void	A_StartDeathCam (objtype *ob) B
{
	s32	dx,dy;
	s32    xmove,ymove;
	s32	dist;
	s16		temp,i;

	FinishPaletteShifts ();

	vid_wait (100);

	if (gamestate.victoryflag)
	{
		playstate = ex_victorious;				// exit castle tile
		return;
	}

	gamestate.victoryflag = TRUE;
	DeathCamIntro ();

//
// line angle up exactly
//
	NewState (player,&s_deathcam);

	player->x = gamestate.killx;
	player->y = gamestate.killy;

	dx = ob->x - player->x;
	dy = player->y - ob->y;

	player->angle = AimAngle (dy,dx);

//
// try to position as close as possible without being in a wall
//
	dist = 0x14000l;
	do
	{
		xmove = FixedByFrac(dist,fcos(player->angle));
		ymove = -FixedByFrac(dist,fsin(player->angle));

		player->x = ob->x - xmove;
		player->y = ob->y - ymove;
		dist += 0x1000;

	} while (!CheckPosition (player));
	plux = player->x >> UNSIGNEDSHIFT;			// scale to fit in u16
	pluy = player->y >> UNSIGNEDSHIFT;
	player->tilex = player->x >> TILESHIFT;		// scale to tile values
	player->tiley = player->y >> TILESHIFT;

//
// go back to the game
//
	fizzlein = TRUE;
	switch (ob->obclass)
	{
	}

}

