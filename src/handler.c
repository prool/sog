/*
 * $Id: handler.c,v 1.135 1999/04/19 14:09:13 fjoe Exp $
 */

/***************************************************************************
 *     ANATOLIA 2.1 is copyright 1996-1997 Serdar BULUT, Ibrahim CANPUNAR  *	
 *     ANATOLIA has been brought to you by ANATOLIA consortium		   *
 *	 Serdar BULUT {Chronos}		bulut@rorqual.cc.metu.edu.tr       *
 *	 Ibrahim Canpunar  {Asena}	canpunar@rorqual.cc.metu.edu.tr    *	
 *	 Murat BICER  {KIO}		mbicer@rorqual.cc.metu.edu.tr	   *	
 *	 D.Baris ACAR {Powerman}	dbacar@rorqual.cc.metu.edu.tr	   *	
 *     By using this code, you have agreed to follow the terms of the      *
 *     ANATOLIA license, in the file Anatolia/anatolia.licence             *	
 ***************************************************************************/

/**************************************************************************r
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1995 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@pacinfo.com)				   *
*	    Gabrielle Taylor (gtaylor@pacinfo.com)			   *
*	    Brian Moore (rom@rom.efn.org)				   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "obj_prog.h"
#include "raffects.h"
#include "fight.h"
#include "quest.h"
#include "db/db.h"
#include "olc/olc.h"
#include "db/lang.h"

DECLARE_DO_FUN(do_raffects	);
DECLARE_DO_FUN(do_return	);
DECLARE_DO_FUN(do_say		);
DECLARE_DO_FUN(do_track		);
DECLARE_DO_FUN(do_look		);

/*
 * Local functions.
 */
void	affect_modify	(CHAR_DATA *ch, AFFECT_DATA *paf, bool fAdd);
int	age_to_num	(int age);

/* friend stuff -- for NPC's mostly */
bool is_friend(CHAR_DATA *ch, CHAR_DATA *victim)
{
	flag64_t off;
	if (is_same_group(ch, victim))
		return TRUE;

	if (!IS_NPC(ch))
		return FALSE;

	off = ch->pIndexData->off_flags;
	if (!IS_NPC(victim)) {
		if (IS_SET(off, ASSIST_PLAYERS))
			return TRUE;
		else
			return FALSE;
	}

	if (IS_AFFECTED(ch, AFF_CHARM))
		return FALSE;

	if (IS_SET(off, ASSIST_ALL))
		return TRUE;

	if (ch->group && ch->group == victim->group)
		return TRUE;

	if (IS_SET(off, ASSIST_VNUM) 
	&&  ch->pIndexData == victim->pIndexData)
		return TRUE;

	if (IS_SET(off, ASSIST_RACE) && ch->race == victim->race)
		return TRUE;
	 
	if (IS_SET(off, ASSIST_ALIGN)
	&&  !IS_SET(ch->pIndexData->act, ACT_NOALIGN)
	&&  !IS_SET(victim->pIndexData->act, ACT_NOALIGN)
	&&  ((IS_GOOD(ch) && IS_GOOD(victim)) ||
	     (IS_EVIL(ch) && IS_EVIL(victim)) ||
	     (IS_NEUTRAL(ch) && IS_NEUTRAL(victim))))
		return TRUE;

	return FALSE;
}

/*
 * Room record:
 * For less than 5 people in room create a new record.
 * Else use the oldest one.
 */

void room_record(const char *name,ROOM_INDEX_DATA *room,int door)
{
  ROOM_HISTORY_DATA *rec;
  int i;

  for (i=0,rec = room->history;i < 5 && rec != NULL;
	   i++,rec = rec->next);

  if (i < 5) {
	rec = calloc(1, sizeof(*rec)); 
	rec->next = room->history;
	if (rec->next != NULL)
	  rec->next->prev = rec; 
	room->history = rec; 
	rec->name = NULL;
  }
  else { 
	rec = room->history->next->next->next->next; 
	rec->prev->next = NULL; 
	rec->next = room->history; 
	rec->next->prev = rec; 
	room->history = rec; 
  }
  rec->prev = NULL;

  if(rec->name) {
	 free_string(rec->name);
  }


  rec->name = str_dup(name);
  rec->went = door;
}

/* returns number of people on an object */
int count_users(OBJ_DATA *obj)
{
	CHAR_DATA *fch;
	int count = 0;

	if (obj->in_room == NULL)
		return 0;

	for (fch = obj->in_room->people; fch != NULL; fch = fch->next_in_room)
		if (fch->on == obj)
		    count++;

	return count;
}

/*
 * Check the material
 */
bool check_material(OBJ_DATA *obj, char *material)
{
	return strstr(obj->material, material) ? TRUE : FALSE;
}

bool is_metal(OBJ_DATA *obj)
{

  if (check_material(obj, "silver") ||
	   check_material(obj, "gold") ||
	   check_material(obj, "iron") ||
	   check_material(obj, "mithril") ||
	   check_material(obj, "adamantite") ||
	   check_material(obj, "steel") ||
	   check_material(obj, "lead") ||
	   check_material(obj, "bronze") ||
	   check_material(obj, "copper") ||
	   check_material(obj, "brass") ||
	   check_material(obj, "platinium") ||
	   check_material(obj, "titanium") ||
	   check_material(obj, "aliminum"))
	return TRUE;

  return FALSE;

}

bool may_float(OBJ_DATA *obj)
{

	if (check_material(obj, "wood")  ||
	     check_material(obj, "ebony")  ||
	     check_material(obj, "ice")  ||
	     check_material(obj, "energy")  ||
	     check_material(obj, "hardwood")  ||
	     check_material(obj, "softwood")  ||
	     check_material(obj, "flesh")  ||
	     check_material(obj, "silk")  ||
	     check_material(obj, "wool")  ||
	     check_material(obj, "cloth")  ||
	     check_material(obj, "fur")  ||
	     check_material(obj, "water")  ||
	     check_material(obj, "ice")  ||
	     check_material(obj, "oak"))
	   return TRUE;

	if (obj->pIndexData->item_type == ITEM_BOAT) 
		return TRUE;

	return FALSE;
}


bool cant_float(OBJ_DATA *obj)
{
	if (check_material(obj, "steel") ||
	     check_material(obj, "iron") ||
	     check_material(obj, "brass") ||
	     check_material(obj, "silver") ||
	     check_material(obj, "gold") ||
	     check_material(obj, "ivory") ||
	     check_material(obj, "copper") ||
	     check_material(obj, "diamond") ||
	     check_material(obj, "pearl") ||
	     check_material(obj, "gem") ||
	     check_material(obj, "platinium") ||
	     check_material(obj, "ruby") ||
	     check_material(obj, "bronze") ||
	     check_material(obj, "titanium") ||
	     check_material(obj, "mithril") ||
	     check_material(obj, "obsidian") ||
	     check_material(obj, "lead"))
	   return TRUE;

	return FALSE;
}

int floating_time(OBJ_DATA *obj)
{
 int  ftime;

 ftime = 0;
 switch(obj->pIndexData->item_type)  
 {
	default: break;
	case ITEM_KEY 	: ftime = 1;	break;
	case ITEM_ARMOR 	: ftime = 2;	break;
	case ITEM_TREASURE 	: ftime = 2;	break;
	case ITEM_PILL 	: ftime = 2;	break;
	case ITEM_POTION 	: ftime = 3;	break;
	case ITEM_TRASH 	: ftime = 3;	break;
	case ITEM_FOOD 	: ftime = 4;	break;
	case ITEM_CONTAINER	: ftime = 5;	break;
	case ITEM_CORPSE_NPC: ftime = 10;	break;
	case ITEM_CORPSE_PC	: ftime = 10;	break;
 }
 ftime = number_fuzzy(ftime) ;

 return (ftime < 0 ? 0 : ftime);
}

/* for immunity, vulnerabiltiy, and resistant
   the 'globals' (magic and weapons) may be overriden
   three other cases -- wood, silver, and iron -- are checked in fight.c */

int check_immune(CHAR_DATA *ch, int dam_type)
{
	int immune, def;
	int bit;

	immune = -1;
	def = IS_NORMAL;

	if (dam_type == DAM_NONE)
		return immune;

	if (dam_type <= 3)
	{
		if (IS_SET(ch->imm_flags,IMM_WEAPON))
		    def = IS_IMMUNE;
		else if (IS_SET(ch->res_flags,RES_WEAPON))
		    def = IS_RESISTANT;
		else if (IS_SET(ch->vuln_flags,VULN_WEAPON))
		    def = IS_VULNERABLE;
	}
	else /* magical attack */
	{	
		if (IS_SET(ch->imm_flags,IMM_MAGIC))
		    def = IS_IMMUNE;
		else if (IS_SET(ch->res_flags,RES_MAGIC))
		    def = IS_RESISTANT;
		else if (IS_SET(ch->vuln_flags,VULN_MAGIC))
		    def = IS_VULNERABLE;
	}

	/* set bits to check -- VULN etc. must ALL be the same or this will fail */
	switch (dam_type)
	{
		case(DAM_BASH):		bit = IMM_BASH;		break;
		case(DAM_PIERCE):	bit = IMM_PIERCE;	break;
		case(DAM_SLASH):	bit = IMM_SLASH;	break;
		case(DAM_FIRE):		bit = IMM_FIRE;		break;
		case(DAM_COLD):		bit = IMM_COLD;		break;
		case(DAM_LIGHTNING):	bit = IMM_LIGHTNING;	break;
		case(DAM_ACID):		bit = IMM_ACID;		break;
		case(DAM_POISON):	bit = IMM_POISON;	break;
		case(DAM_NEGATIVE):	bit = IMM_NEGATIVE;	break;
		case(DAM_HOLY):		bit = IMM_HOLY;		break;
		case(DAM_ENERGY):	bit = IMM_ENERGY;	break;
		case(DAM_MENTAL):
			if (IS_IMMORTAL(ch))
				return IS_IMMUNE;
			bit = IMM_MENTAL;
			break;
		case(DAM_DISEASE):	bit = IMM_DISEASE;	break;
		case(DAM_DROWNING):	bit = IMM_DROWNING;	break;
		case(DAM_LIGHT):	bit = IMM_LIGHT;	break;
		case(DAM_CHARM):
			if (IS_IMMORTAL(ch))
				return IS_IMMUNE;
			bit = IMM_CHARM;
			break;
		case(DAM_SOUND):	bit = IMM_SOUND;	break;
		default:		return def;
	}

	if (IS_SET(ch->imm_flags, bit))
		immune = IS_IMMUNE;
	else if (IS_SET(ch->res_flags, bit) && immune != IS_IMMUNE)
		immune = IS_RESISTANT;
	else if (IS_SET(ch->vuln_flags, bit)) {
		if (immune == IS_IMMUNE)
		    immune = IS_RESISTANT;
		else if (immune == IS_RESISTANT)
		    immune = IS_NORMAL;
		else
		    immune = IS_VULNERABLE;
	}

	if (!IS_NPC(ch) && get_curr_stat(ch, STAT_CHA) < 18 
			&& dam_type == DAM_CHARM)
		immune = IS_VULNERABLE;

	if (immune == -1)
		return def;
	else
	  	return immune;
}

void reset_obj_affects(CHAR_DATA *ch, OBJ_DATA *obj, AFFECT_DATA *af)
{
	for (; af != NULL; af = af->next) {
		int mod = af->modifier;

		switch(af->location) {
		case APPLY_MANA:
			ch->max_mana	-= mod;
			break;
		case APPLY_HIT:
			ch->max_hit	-= mod;
			break;
		case APPLY_MOVE:
			ch->max_move	-= mod;
			break;
		}
	}
}

/* used to de-screw characters */
void reset_char(CHAR_DATA *ch)
{
	 int loc,mod,stat;
	 OBJ_DATA *obj;
	 AFFECT_DATA *af;
	 int i;

	 if (IS_NPC(ch))
		return;

	if (ch->pcdata->perm_hit == 0 
	||  ch->pcdata->perm_mana == 0
	||  ch->pcdata->perm_move == 0
	||  ch->pcdata->last_level == 0) {
	/* do a FULL reset */
		for (loc = 0; loc < MAX_WEAR; loc++) {
			obj = get_eq_char(ch,loc);
			if (obj == NULL)
				continue;
			if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED))
				reset_obj_affects(ch, obj,
						  obj->pIndexData->affected);
			reset_obj_affects(ch, obj, obj->affected);
	        }

		/* now reset the permanent stats */
		ch->pcdata->perm_hit 	= ch->max_hit;
		ch->pcdata->perm_mana 	= ch->max_mana;
		ch->pcdata->perm_move	= ch->max_move;
		ch->pcdata->last_level	= ch->played/3600;
		if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
			if (ch->sex > 0 && ch->sex < 3)
		    	    ch->pcdata->true_sex	= ch->sex;
			else
			    ch->pcdata->true_sex 	= 0;
	}

	/* now restore the character to his/her true condition */
	for (stat = 0; stat < MAX_STATS; stat++)
		ch->mod_stat[stat] = 0;

	if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
		ch->pcdata->true_sex = 0; 
	ch->sex		= ch->pcdata->true_sex;
	ch->max_hit 	= ch->pcdata->perm_hit;
	ch->max_mana	= ch->pcdata->perm_mana;
	ch->max_move	= ch->pcdata->perm_move;
   
	for (i = 0; i < 4; i++)
		ch->armor[i]	= 100;

	ch->hitroll		= 0;
	ch->damroll		= 0;
	ch->saving_throw	= 0;
	ch->drain_level		= 0;

	/* now start adding back the effects */
	for (loc = 0; loc < MAX_WEAR; loc++)
	{
	    obj = get_eq_char(ch,loc);
	    if (obj == NULL)
	        continue;
		for (i = 0; i < 4; i++)
		    ch->armor[i] -= apply_ac(obj, loc, i);

	    if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED))
		for (af = obj->pIndexData->affected; af != NULL; af = af->next)
	    {
	        mod = af->modifier;
	        switch(af->location)
	        {
			case APPLY_STR:		ch->mod_stat[STAT_STR]	+= mod;	break;
			case APPLY_DEX:		ch->mod_stat[STAT_DEX]	+= mod; break;
			case APPLY_INT:		ch->mod_stat[STAT_INT]	+= mod; break;
			case APPLY_WIS:		ch->mod_stat[STAT_WIS]	+= mod; break;
			case APPLY_CON:		ch->mod_stat[STAT_CON]	+= mod; break;
 		case APPLY_CHA:		ch->mod_stat[STAT_CHA]	+= mod; break;

			case APPLY_MANA:	ch->max_mana		+= mod; break;
			case APPLY_HIT:		ch->max_hit		+= mod; break;
			case APPLY_MOVE:	ch->max_move		+= mod; break;
			case APPLY_AGE:		ch->played += age_to_num(mod); break;
			case APPLY_AC:		
			    for (i = 0; i < 4; i ++)
				ch->armor[i] += mod; 
			    break;
			case APPLY_HITROLL:	ch->hitroll		+= mod; break;
			case APPLY_DAMROLL:	ch->damroll		+= mod; break;
			case APPLY_SIZE:	ch->size		+= mod; break;
			case APPLY_LEVEL:	ch->drain_level		+= mod; break;
			case APPLY_SAVES:		ch->saving_throw += mod; break;
			case APPLY_SAVING_ROD: 		ch->saving_throw += mod; break;
			case APPLY_SAVING_PETRI:	ch->saving_throw += mod; break;
			case APPLY_SAVING_BREATH: 	ch->saving_throw += mod; break;
			case APPLY_SAVING_SPELL:	ch->saving_throw += mod; break;
		    }
	    }
 
	    for (af = obj->affected; af != NULL; af = af->next)
	    {
	        mod = af->modifier;
	        switch(af->location)
	        {
	            case APPLY_STR:         ch->mod_stat[STAT_STR]  += mod; break;
	            case APPLY_DEX:         ch->mod_stat[STAT_DEX]  += mod; break;
	            case APPLY_INT:         ch->mod_stat[STAT_INT]  += mod; break;
	            case APPLY_WIS:         ch->mod_stat[STAT_WIS]  += mod; break;
	            case APPLY_CON:         ch->mod_stat[STAT_CON]  += mod; break;
			case APPLY_CHA:		ch->mod_stat[STAT_CHA]	+= mod; break;
 
	            case APPLY_MANA:        ch->max_mana            += mod; break;
	            case APPLY_HIT:         ch->max_hit             += mod; break;
	            case APPLY_MOVE:        ch->max_move            += mod; break;
			case APPLY_AGE:		ch->played += age_to_num(mod); break;
 
	            case APPLY_AC:
	                for (i = 0; i < 4; i ++)
	                    ch->armor[i] += mod;
	                break;
			case APPLY_HITROLL:     ch->hitroll             += mod; break;
	            case APPLY_DAMROLL:     ch->damroll             += mod; break;
 		case APPLY_SIZE:	ch->size		+= mod; break;
		case APPLY_LEVEL:	ch->drain_level		+= mod; break;
	            case APPLY_SAVES:	ch->saving_throw	+= mod; break;
	            case APPLY_SAVING_ROD:          ch->saving_throw += mod; break;
	            case APPLY_SAVING_PETRI:        ch->saving_throw += mod; break;
	            case APPLY_SAVING_BREATH:       ch->saving_throw += mod; break;
	            case APPLY_SAVING_SPELL:        ch->saving_throw += mod; break;
	        }
		}
	}
  
	/* now add back spell effects */
	for (af = ch->affected; af != NULL; af = af->next)
	{
	    mod = af->modifier;
	    switch(af->location)
	    {
	            case APPLY_STR:         ch->mod_stat[STAT_STR]  += mod; break;
	            case APPLY_DEX:         ch->mod_stat[STAT_DEX]  += mod; break;
	            case APPLY_INT:         ch->mod_stat[STAT_INT]  += mod; break;
	            case APPLY_WIS:         ch->mod_stat[STAT_WIS]  += mod; break;
	            case APPLY_CON:         ch->mod_stat[STAT_CON]  += mod; break;
 		case APPLY_CHA:		ch->mod_stat[STAT_CHA]	+= mod; break;

	            case APPLY_MANA:        ch->max_mana            += mod; break;
	            case APPLY_HIT:         ch->max_hit             += mod; break;
	            case APPLY_MOVE:        ch->max_move            += mod; break;
 
	            case APPLY_AC:
	                for (i = 0; i < 4; i ++)
	                    ch->armor[i] += mod;
	                break;
	            case APPLY_HITROLL:     ch->hitroll             += mod; break;
	            case APPLY_DAMROLL:     ch->damroll             += mod; break;
 		case APPLY_SIZE:	ch->size		+= mod; break;
		case APPLY_LEVEL:	ch->drain_level		+= mod; break;
	            case APPLY_SAVES:	ch->saving_throw	+= mod; break;
	            case APPLY_SAVING_ROD:          ch->saving_throw += mod; break;
	            case APPLY_SAVING_PETRI:        ch->saving_throw += mod; break;
	            case APPLY_SAVING_BREATH:       ch->saving_throw += mod; break;
	            case APPLY_SAVING_SPELL:        ch->saving_throw += mod; break;
	    } 
	}
	/* make sure sex is RIGHT! */
	if (ch->sex < 0 || ch->sex > 2)
		ch->sex = ch->pcdata->true_sex;
 
}

/*
 * Retrieve a character's age.
 */
int get_age(CHAR_DATA *ch)
{
	return 17 + (ch->played + (int) (current_time - ch->logon)) / 72000;
}

int age_to_num(int age)
{
	return  age * 72000;
}

/*
 * Retrieve a character's carry capacity.
 */
int can_carry_n(CHAR_DATA *ch)
{
	if (IS_IMMORTAL(ch))
		return 1000;

	if (IS_NPC(ch) && IS_SET(ch->pIndexData->act, ACT_PET))
		return 0;

	return MAX_WEAR + get_curr_stat(ch,STAT_DEX) - 10 + ch->size;
}

/*
 * Retrieve a character's carry capacity.
 */
int can_carry_w(CHAR_DATA *ch)
{
	if (IS_IMMORTAL(ch))
		return 10000000;

	if (IS_NPC(ch) && IS_SET(ch->pIndexData->act, ACT_PET))
		return 0;

	return str_app[get_curr_stat(ch,STAT_STR)].carry * 10 + ch->level * 25;
}

/*---------------------------------------------------------------------------
 * name list stuff
 *
 * name list is simply string of names
 * separated by spaces. if name contains spaces itself it is enclosed
 * in single quotes
 *
 */

/*
 * See if a string is one of the names of an object.
 */
bool is_name_raw(const char *str, const char *namelist,
		 int (*cmpfun)(const char*, const char*))
{
	char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
	const char *list, *string;
	
	if (IS_NULLSTR(namelist) || IS_NULLSTR(str))
		return FALSE;

	if (!str_cmp(namelist, "all"))
		return TRUE;

	string = str;
	/* we need ALL parts of string to match part of namelist */
	for (; ;) { /* start parsing string */
		str = one_argument(str, part, sizeof(part));

		if (part[0] == '\0')
			return TRUE;

		/* check to see if this is part of namelist */
		list = namelist;
		for (; ;) { /* start parsing namelist */
			list = one_argument(list, name, sizeof(name));
			if (name[0] == '\0')  /* this name was not found */
				return FALSE;

			if (!cmpfun(string, name))
				return TRUE; /* full pattern match */

			if (!cmpfun(part, name))
				break;
		}
	}
}

bool is_name(const char *str, const char *namelist)
{
	return is_name_raw(str, namelist, str_prefix);
}

void cat_name(char *buf, const char *name, size_t len)
{
	bool have_spaces = strpbrk(name, " \t") != NULL;

	if (buf[0])
		strnzcat(buf, len, " ");
	if (have_spaces)
		strnzcat(buf, len, "'");
	strnzcat(buf, len, name);
	if (have_spaces)
		strnzcat(buf, len, "'");
}

/* 
 * name_edit flags
 */
#define NE_F_DELETE	(A)	/* delete name if found		*/
#define NE_F_ADD	(B)	/* add name if not found	*/

/*
 * name_edit - edit 'name' according to 'flags' in name list pointed by 'nl'
 *             if ch == NULL name_edit will be silent
 *	       (and 'editor_name' is not used)
 * Return values: TRUE  - name was found in namelist
 *		  FALSE - name was not found
 *
 */
bool name_edit(const char **nl, const char *name, int flags,
	       CHAR_DATA *ch, const char *editor_name)
{
	bool found = FALSE;
	const char *p = *nl;
	char buf[MAX_STRING_LENGTH];

	buf[0] = '\0';
	for (;;) {
		char arg[MAX_STRING_LENGTH];

		p = first_arg(p, arg, sizeof(arg), FALSE);

		if (arg[0] == '\0')
			break;

		if (!str_cmp(name, arg)) {
			found = TRUE;
			if (IS_SET(flags, NE_F_DELETE))
				continue;
		}

		cat_name(buf, arg, sizeof(buf));
	}

	if (!found) {
		if (!IS_SET(flags, NE_F_ADD))
			return found;

		if (strlen(buf) + strlen(name) + 4 > MAX_STRING_LENGTH) {
			if (ch)
				char_printf(ch, "%s: name list too long\n",
					    editor_name);
			return found;
		}
		cat_name(buf, name, sizeof(buf));
		if (ch)
			char_printf(ch, "%s: %s: name added.\n",
				    editor_name, name);
	}
	else {
		if (!IS_SET(flags, NE_F_DELETE))
			return found;

		if (ch)
			char_printf(ch, "%s: %s: name removed.\n",
				    editor_name, name);
	}

	free_string(*nl);
	*nl = str_dup(buf);
	return found;
}

bool name_add(const char **nl, const char *name,
	      CHAR_DATA *ch, const char *editor_name)
{
	return name_edit(nl, name, NE_F_ADD, ch, editor_name);
}

bool name_delete(const char **nl, const char *name,
		 CHAR_DATA *ch, const char *editor_name)
{
	return name_edit(nl, name, NE_F_DELETE, ch, editor_name);
}

bool name_toggle(const char **nl, const char *name,
		 CHAR_DATA *ch, const char *editor_name)
{
	if (!str_cmp(name, "all")) {
		free_string(*nl);
		*nl = str_dup(name);
		if (ch)
			char_printf(ch, "%s: name list set to ALL.\n",
				    editor_name);
		return TRUE;
	}

	if (!str_cmp(name, "none")) {
		free_string(*nl);
		*nl = str_empty;
		if (ch)
			char_printf(ch, "%s: name list reset.\n", editor_name);
		return TRUE;
	}

	if (!str_cmp(*nl, "all")) {
		free_string(*nl);
		*nl = str_empty;
	}

	return name_edit(nl, name, NE_F_ADD | NE_F_DELETE, ch, editor_name);
}

/* enchanted stuff for eq */
void affect_enchant(OBJ_DATA *obj)
{
	/* okay, move all the old flags into new vectors if we have to */
	if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED)) {
	    AFFECT_DATA *paf, *af_new;
	    SET_BIT(obj->extra_flags, ITEM_ENCHANTED);

	    for (paf = obj->pIndexData->affected;
	         paf != NULL; paf = paf->next)
	    {
		    af_new = aff_new();

	        af_new->next = obj->affected;
	        obj->affected = af_new;
 
		    af_new->where	= paf->where;
	        af_new->type        = UMAX(0,paf->type);
	        af_new->level       = paf->level;
	        af_new->duration    = paf->duration;
	        af_new->location    = paf->location;
	        af_new->modifier    = paf->modifier;
	        af_new->bitvector   = paf->bitvector;
	    }
	}
}
	       

/*
 * Apply or remove an affect to a character.
 */
void affect_modify(CHAR_DATA *ch, AFFECT_DATA *paf, bool fAdd)
{
	OBJ_DATA *wield, *obj2;
	int mod, i;

	mod = paf->modifier;
	if (fAdd) {
		switch (paf->where) {
		case TO_AFFECTS:
			SET_BIT(ch->affected_by, paf->bitvector);
			break;
		case TO_IMMUNE:
			SET_BIT(ch->imm_flags, paf->bitvector);
			break;
		case TO_RESIST:
			SET_BIT(ch->res_flags, paf->bitvector);
			break;
		case TO_VULN:
			SET_BIT(ch->vuln_flags, paf->bitvector);
			break;
		}
	}
	else {
		switch (paf->where) {
		case TO_AFFECTS:
			REMOVE_BIT(ch->affected_by, paf->bitvector);
			break;
		case TO_IMMUNE:
			REMOVE_BIT(ch->imm_flags, paf->bitvector);
			break;
		case TO_RESIST:
			REMOVE_BIT(ch->res_flags, paf->bitvector);
			break;
		case TO_VULN:
	        	REMOVE_BIT(ch->vuln_flags, paf->bitvector);
	        	break;
		}
		mod = 0 - mod;
	}

	switch (paf->location) {
	case APPLY_NONE:
	case APPLY_CLASS:
	case APPLY_HEIGHT:
	case APPLY_WEIGHT:
	case APPLY_GOLD:
	case APPLY_EXP:
	case APPLY_SEX:
	case APPLY_SPELL_AFFECT:
		break;

	case APPLY_STR:		ch->mod_stat[STAT_STR]	+= mod; break;
	case APPLY_DEX:		ch->mod_stat[STAT_DEX]	+= mod;	break;
	case APPLY_INT:		ch->mod_stat[STAT_INT]	+= mod;	break;
	case APPLY_WIS:		ch->mod_stat[STAT_WIS]	+= mod;	break;
	case APPLY_CON:		ch->mod_stat[STAT_CON]	+= mod;	break;
	case APPLY_CHA:		ch->mod_stat[STAT_CHA]	+= mod; break;

	case APPLY_MANA:	ch->max_mana		+= mod;	break;
	case APPLY_HIT:		ch->max_hit		+= mod;	break;
	case APPLY_MOVE:	ch->max_move		+= mod;	break;

	case APPLY_HITROLL:	ch->hitroll		+= mod;	break;
	case APPLY_DAMROLL:	ch->damroll		+= mod;	break;
	case APPLY_LEVEL:	ch->drain_level		+= mod; break;

	case APPLY_SIZE:	ch->size	+= mod;			break;
	case APPLY_AGE:		ch->played	+= age_to_num(mod);	break;

	case APPLY_AC:
		for (i = 0; i < 4; i ++)
			ch->armor[i] += mod;
		break;

	case APPLY_SAVES:		ch->saving_throw	+= mod;	break;
	case APPLY_SAVING_ROD:		ch->saving_throw	+= mod;	break;
	case APPLY_SAVING_PETRI:	ch->saving_throw	+= mod;	break;
	case APPLY_SAVING_BREATH:	ch->saving_throw	+= mod;	break;
	case APPLY_SAVING_SPELL:	ch->saving_throw	+= mod;	break;

	case APPLY_RACE: {
		int from;
		int to;
		race_t *rto;
		race_t *rfrom;

		if (fAdd) {
			from = ORG_RACE(ch);
			to = ch->race = paf->modifier;
		}
		else {
			from = ch->race;
			to = ch->race = ORG_RACE(ch);
		}

		rfrom = race_lookup(from);
		rto = race_lookup(to);
		if (!rfrom || !rto || !rfrom->pcdata || !rto->pcdata)
			return;

		REMOVE_BIT(ch->affected_by, rfrom->aff);
		SET_BIT(ch->affected_by, rto->aff);
		affect_check(ch, TO_AFFECTS, rfrom->aff);

		REMOVE_BIT(ch->imm_flags, rfrom->imm);
		SET_BIT(ch->imm_flags, rto->imm);
		affect_check(ch, TO_IMMUNE, rfrom->imm);

		REMOVE_BIT(ch->res_flags, rfrom->res);
		SET_BIT(ch->res_flags, rto->res);
		affect_check(ch, TO_RESIST, rfrom->res);

		REMOVE_BIT(ch->vuln_flags, rfrom->vuln);
		SET_BIT(ch->vuln_flags, rto->vuln);
		affect_check(ch, TO_VULN, rfrom->vuln);

		ch->form = rto->form;
		ch->parts = rto->parts;
		ch->size = rto->pcdata->size;
		update_skills(ch);
		break;
	}
	default:
		if (IS_NPC(ch)) {
			log_printf("affect_modify: vnum %d: in room %d: "
				   "unknown location %d.",
				   ch->pIndexData->vnum,
				   ch->in_room ? ch->in_room->vnum : -1,
				   paf->location);
		}
		else {
			log_printf("affect_modify: %s: unknown location %d.",
				   ch->name, paf->location);
		}
		return;

	}

	/*
	 * Check for weapon wielding.
	 * Guard against recursion (for weapons with affects).
	 */
	if (!IS_NPC(ch) && (wield = get_eq_char(ch, WEAR_WIELD)) != NULL
	&&  get_obj_weight(wield) > (str_app[get_curr_stat(ch,STAT_STR)].wield*10))
	{
		static int depth;

		if (depth == 0) {
		    depth++;
		    act("You drop $p.", ch, wield, NULL, TO_CHAR);
		    act("$n drops $p.", ch, wield, NULL, TO_ROOM);
		    obj_from_char(wield);
		    obj_to_room(wield, ch->in_room);

		if ((obj2 = get_eq_char(ch, WEAR_SECOND_WIELD)) != NULL) {
			act("You wield his second weapon as your first!",  ch, NULL,NULL,TO_CHAR);
			act("$n wields his second weapon as first!",  ch, NULL,NULL,TO_ROOM);
		unequip_char(ch, obj2);
		equip_char(ch, obj2 , WEAR_WIELD);
		}
		    depth--;
		}
	}
}


/* find an effect in an affect list */
AFFECT_DATA  *affect_find(AFFECT_DATA *paf, int sn)
{
	AFFECT_DATA *paf_find;
	
	for (paf_find = paf; paf_find != NULL; paf_find = paf_find->next)
	{
	    if (paf_find->type == sn)
		return paf_find;
	}

	return NULL;
}

void affect_check_list(CHAR_DATA *ch, AFFECT_DATA *paf,
		       int where, flag64_t vector)
{
	for (; paf; paf = paf->next)
		if ((where < 0 || paf->where == where)
		&&  (paf->bitvector & vector))
			switch (paf->where) {
			case TO_AFFECTS:
				SET_BIT(ch->affected_by, paf->bitvector);
				break;
			case TO_IMMUNE:
				SET_BIT(ch->imm_flags, paf->bitvector);   
				break;
			case TO_RESIST:
				SET_BIT(ch->res_flags, paf->bitvector);
				break;
			case TO_VULN:
				SET_BIT(ch->vuln_flags, paf->bitvector);
				break;
			}
}

/* fix object affects when removing one */
void affect_check(CHAR_DATA *ch, int where, flag64_t vector)
{
	OBJ_DATA *obj;

	if (where == TO_OBJECT || where == TO_WEAPON || vector == 0)
		return;

	affect_check_list(ch, ch->affected, where, vector);
	for (obj = ch->carrying; obj != NULL; obj = obj->next_content) {
		if (obj->wear_loc == -1 || obj->wear_loc == WEAR_STUCK_IN)
			continue;
		affect_check_list(ch, obj->affected, where, vector);

		if (IS_SET(obj->extra_flags, ITEM_ENCHANTED))
			continue;

		affect_check_list(ch, obj->pIndexData->affected, where, vector);
	}
}

/*
 * Give an affect to a char.
 */
void affect_to_char(CHAR_DATA *ch, AFFECT_DATA *paf)
{
	AFFECT_DATA *paf_new;

	paf_new = aff_new();

	*paf_new	= *paf;
	paf_new->next	= ch->affected;
	ch->affected	= paf_new;

	affect_modify(ch, paf_new, TRUE);
}

/* give an affect to an object */
void affect_to_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
	AFFECT_DATA *paf_new;

	paf_new = aff_new();
	*paf_new	= *paf;
	paf_new->next	= obj->affected;
	obj->affected	= paf_new;

	/* apply any affect vectors to the object's extra_flags */
	if (paf->bitvector)
		switch (paf->where) {
		case TO_OBJECT:
			SET_BIT(obj->extra_flags,paf->bitvector);
			break;
		case TO_WEAPON:
			if (obj->pIndexData->item_type == ITEM_WEAPON)
		        	SET_BIT(obj->value[4],paf->bitvector);
			break;
		}
}



/*
 * Remove an affect from a char.
 */
void affect_remove(CHAR_DATA *ch, AFFECT_DATA *paf)
{
	int where;
	int vector;

	if (ch->affected == NULL) {
		bug("Affect_remove: no affect.", 0);
		return;
	}

	affect_modify(ch, paf, FALSE);
	where = paf->where;
	vector = paf->bitvector;

	if (paf == ch->affected)
		ch->affected	= paf->next;
	else {
		AFFECT_DATA *prev;

		for (prev = ch->affected; prev; prev = prev->next) {
			if (prev->next == paf) {
				prev->next = paf->next;
				break;
			}
		}

		if (prev == NULL) {
			bug("Affect_remove: cannot find paf.", 0);
			return;
		}
	}

	aff_free(paf);
	affect_check(ch, where, vector);
}

void affect_remove_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
	int where, vector;

	if (obj->affected == NULL)
		return;

	if (obj->carried_by != NULL && obj->wear_loc != -1)
		affect_modify(obj->carried_by, paf, FALSE);

	where = paf->where;
	vector = paf->bitvector;

	/* remove flags from the object if needed */
	if (paf->bitvector)
		switch(paf->where) {
		case TO_OBJECT:
			REMOVE_BIT(obj->extra_flags,paf->bitvector);
			break;
		case TO_WEAPON:
			if (obj->pIndexData->item_type == ITEM_WEAPON)
				REMOVE_BIT(obj->value[4],paf->bitvector);
			break;
		}

	if (paf == obj->affected)
	    obj->affected    = paf->next;
	else
	{
	    AFFECT_DATA *prev;

	    for (prev = obj->affected; prev != NULL; prev = prev->next)
	    {
	        if (prev->next == paf)
	        {
	            prev->next = paf->next;
	            break;
	        }
	    }

	    if (prev == NULL)
	    {
	        bug("Affect_remove_object: cannot find paf.", 0);
	        return;
	    }
	}

	aff_free(paf);

	if (obj->carried_by != NULL && obj->wear_loc != -1)
		affect_check(obj->carried_by,where,vector);
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip(CHAR_DATA *ch, int sn)
{
	AFFECT_DATA *paf;
	AFFECT_DATA *paf_next;

	for (paf = ch->affected; paf; paf = paf_next) {
		paf_next = paf->next;
		if (paf->type == sn)
			affect_remove(ch, paf);
	}
}

/*
 * strip all affects which affect given bitvector
 */
void affect_bit_strip(CHAR_DATA *ch, int where, flag64_t bits)
{
	AFFECT_DATA *paf;
	AFFECT_DATA *paf_next;

	for (paf = ch->affected; paf; paf = paf_next) {
		paf_next = paf->next;
		if (paf->where == where && (paf->bitvector & bits))
			affect_remove(ch, paf);
	}
}

/*
 * Return true if a char is affected by a spell.
 */
bool is_affected(CHAR_DATA *ch, int sn)
{
	AFFECT_DATA *paf;

	for (paf = ch->affected; paf; paf = paf->next)
		if (paf->type == sn)
			return TRUE;

	return FALSE;
}

bool is_bit_affected(CHAR_DATA *ch, int where, flag64_t bits)
{
	AFFECT_DATA *paf;

	for (paf = ch->affected; paf; paf = paf->next)
		if (paf->where == where && (paf->bitvector & bits))
			return TRUE;

	return FALSE;
}

bool has_obj_affect(CHAR_DATA *ch, int vector)
{
	OBJ_DATA *obj;

	for (obj = ch->carrying; obj; obj = obj->next_content) {
		AFFECT_DATA *paf;

		if (obj->wear_loc == -1 || obj->wear_loc == WEAR_STUCK_IN)
			continue;

		for (paf = obj->affected; paf; paf = paf->next)
	        	if (paf->bitvector & vector)
				return TRUE;

		if (IS_SET(obj->extra_flags, ITEM_ENCHANTED))
			continue;

		for (paf = obj->pIndexData->affected; paf; paf = paf->next)
			if (paf->bitvector & vector)
				return TRUE;
	}
	return FALSE;
}

/*
 * Add or enhance an affect.
 */
void affect_join(CHAR_DATA *ch, AFFECT_DATA *paf)
{
	AFFECT_DATA *paf_old;
	bool found;

	found = FALSE;
	for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next)
	{
		if (paf_old->type == paf->type)
		{
		    paf->level = (paf->level += paf_old->level) / 2;
		    paf->duration += paf_old->duration;
		    paf->modifier += paf_old->modifier;
		    affect_remove(ch, paf_old);
		    break;
		}
	}

	affect_to_char(ch, paf);
	return;
}

/*
 * Move a char out of a room.
 */
void char_from_room(CHAR_DATA *ch)
{
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *prev_room = ch->in_room;

	if (ch->in_room == NULL) {
		bug("Char_from_room: NULL.", 0);
		return;
	}

	if (!IS_NPC(ch))
		--ch->in_room->area->nplayer;

	if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
	&&   obj->pIndexData->item_type == ITEM_LIGHT
	&&   obj->value[2] != 0
	&&   ch->in_room->light > 0)
		--ch->in_room->light;

	if (ch == ch->in_room->people)
		ch->in_room->people = ch->next_in_room;
	else
	{
		CHAR_DATA *prev;

		for (prev = ch->in_room->people; prev; prev = prev->next_in_room)
		{
		    if (prev->next_in_room == ch)
		    {
			prev->next_in_room = ch->next_in_room;
			break;
		    }
		}

		if (prev == NULL)
		    bug("Char_from_room: ch not found.", 0);
	}

	ch->in_room      = NULL;
	ch->next_in_room = NULL;
	ch->on 	     = NULL;  /* sanity check! */

	if (MOUNTED(ch))
	{
	 ch->mount->riding	= FALSE;
	 ch->riding		= FALSE;
	}

	if (RIDDEN(ch))
	{
	 ch->mount->riding	= FALSE;
	 ch->riding		= FALSE;
	}

	if (prev_room && prev_room->affected_by)
		  raffect_back_char(prev_room, ch);

	return;
}

/*
 * Move a char into a room.
 */
void char_to_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
	OBJ_DATA *obj;

	if (pRoomIndex == NULL) {
		ROOM_INDEX_DATA *room;

		bug("Char_to_room: NULL.", 0);
		
		if ((room = get_room_index(ROOM_VNUM_TEMPLE)) != NULL)
			char_to_room(ch, room);
		
		return;
	}

	ch->in_room		= pRoomIndex;
	ch->next_in_room	= pRoomIndex->people;
	pRoomIndex->people	= ch;

	if (!IS_NPC(ch))
	{
		if (ch->in_room->area->empty)
		{
		    ch->in_room->area->empty = FALSE;
		    ch->in_room->area->age = 0;
		}
		++ch->in_room->area->nplayer;
	}

	if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
	&&   obj->pIndexData->item_type == ITEM_LIGHT
	&&   obj->value[2] != 0)
		++ch->in_room->light;
		
	while (IS_AFFECTED(ch,AFF_PLAGUE))
	{
	    AFFECT_DATA *af, plague;
	    CHAR_DATA *vch;
	    
	    for (af = ch->affected; af != NULL; af = af->next)
	    {
	        if (af->type == gsn_plague)
	            break;
	    }
	    
	    if (af == NULL)
	    {
	        REMOVE_BIT(ch->affected_by,AFF_PLAGUE);
	        break;
	    }
	    
	    if (af->level == 1)
	        break;
	    
		plague.where		= TO_AFFECTS;
	    plague.type 		= gsn_plague;
	    plague.level 		= af->level - 1; 
	    plague.duration 	= number_range(1,2 * plague.level);
	    plague.location		= APPLY_STR;
	    plague.modifier 	= -5;
	    plague.bitvector 	= AFF_PLAGUE;
	    
	    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
	    {
	        if (!saves_spell(plague.level - 2,vch,DAM_DISEASE) 
		    &&  !IS_IMMORTAL(vch) &&
	        	!IS_AFFECTED(vch,AFF_PLAGUE) && number_bits(6) == 0)
	        {
	        	char_puts("You feel hot and feverish.\n",vch);
	        	act("$n shivers and looks very ill.",vch,NULL,NULL,TO_ROOM);
	        	affect_join(vch,&plague);
	        }
	    }
		break;
	}

	if (ch->in_room->affected_by) {
		 if (IS_IMMORTAL(ch))
			do_raffects(ch,str_empty);
		 else
			raffect_to_char(ch->in_room, ch);
	}

	if (ch->desc && OLCED(ch) && IS_EDIT(ch, ED_ROOM))
		roomed_edit_room(ch, pRoomIndex, TRUE);
}

/*
 * Give an obj to a char.
 */
void obj_to_char(OBJ_DATA *obj, CHAR_DATA *ch)
{
	obj->next_content	= ch->carrying;
	ch->carrying		= obj;
	obj->carried_by		= ch;
	obj->in_room		= NULL;
	obj->in_obj		= NULL;
	ch->carry_number	+= get_obj_number(obj);
	ch->carry_weight	+= get_obj_weight(obj);
}

/*
 * Take an obj from its character.
 */
void obj_from_char(OBJ_DATA *obj)
{
	CHAR_DATA *ch;

	if ((ch = obj->carried_by) == NULL) {
		bug("Obj_from_char: null ch.", 0);
		log_printf("Name %s", obj->name);
		return;
	}

	if (obj->wear_loc != WEAR_NONE)
		unequip_char(ch, obj);

	if (ch->carrying == obj)
		ch->carrying = obj->next_content;
	else {
		OBJ_DATA *prev;

		for (prev = ch->carrying; prev; prev = prev->next_content) {
			if (prev->next_content == obj) {
				prev->next_content = obj->next_content;
				break;
			}
		}

		if (prev == NULL)
			bug("Obj_from_char: obj not in list.", 0);
	}

	obj->carried_by		= NULL;
	obj->next_content	= NULL;
	ch->carry_number	-= get_obj_number(obj);
	ch->carry_weight	-= get_obj_weight(obj);
}

/*
 * Find the ac value of an obj, including position effect.
 */
int apply_ac(OBJ_DATA *obj, int iWear, int type)
{
	if (obj->pIndexData->item_type != ITEM_ARMOR)
		return 0;

	switch (iWear) {
	case WEAR_BODY:		return 3 * obj->value[type];
	case WEAR_HEAD:		return 2 * obj->value[type];
	case WEAR_LEGS:		return 2 * obj->value[type];
	case WEAR_FEET:		return	obj->value[type];
	case WEAR_HANDS:	return	obj->value[type];
	case WEAR_ARMS:		return	obj->value[type];
	case WEAR_SHIELD:	return	obj->value[type];
	case WEAR_FINGER_L:	return	0;
	case WEAR_FINGER_R:	return	obj->value[type];
	case WEAR_NECK_1:	return	obj->value[type];
	case WEAR_NECK_2:	return	obj->value[type];
	case WEAR_ABOUT:	return 2 * obj->value[type];
	case WEAR_WAIST:	return	obj->value[type];
	case WEAR_WRIST_L:	return	obj->value[type];
	case WEAR_WRIST_R:	return	obj->value[type];
	case WEAR_HOLD:		return	obj->value[type];
	}

	return 0;
}

/*
 * Find a piece of eq on a character.
 */
OBJ_DATA *get_eq_char(CHAR_DATA *ch, int iWear)
{
	OBJ_DATA *obj;

	if (ch == NULL)
		return NULL;

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
		if (obj->wear_loc == iWear)
			return obj;

	return NULL;
}

/*
 * Equip a char with an obj. Return obj on success. Otherwise returns NULL.
 */
OBJ_DATA * equip_char(CHAR_DATA *ch, OBJ_DATA *obj, int iWear)
{
	AFFECT_DATA *paf;
	int i;

	if (iWear == WEAR_STUCK_IN) {
		obj->wear_loc = iWear;
		return obj;
	}

	if (get_eq_char(ch, iWear)) {
		if (IS_NPC(ch)) {
			log_printf("equip_char: vnum %d: in_room %d: "
				   "obj vnum %d: location %s: "
				   "already equipped.",
				   ch->pIndexData->vnum,
				   ch->in_room ? ch->in_room->vnum : -1,
				   obj->pIndexData->vnum,
				   flag_string(wear_loc_flags, iWear));
		}
		else {
			log_printf("equip_char: %s: location %s: "
				   "already equipped.",
				   ch->name,
				   flag_string(wear_loc_flags, iWear));
		}
		return NULL;
	}

	if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL)    && IS_EVIL(ch)   )
	||  (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD)    && IS_GOOD(ch)   )
	||  (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch))) {
		/*
		 * Thanks to Morgenes for the bug fix here!
		 */
		act("You are zapped by $p and drop it.", ch, obj, NULL, TO_CHAR);
		act("$n is zapped by $p and drops it.",  ch, obj, NULL, TO_ROOM);
		obj_from_char(obj);
		obj_to_room(obj, ch->in_room);
		return NULL;
	}

	for (i = 0; i < 4; i++)
		ch->armor[i]      	-= apply_ac(obj, iWear,i);
	obj->wear_loc	 = iWear;

	if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED))
		for (paf = obj->pIndexData->affected; paf; paf = paf->next)
			if (paf->location != APPLY_SPELL_AFFECT)
				affect_modify(ch, paf, TRUE);
	for (paf = obj->affected; paf; paf = paf->next)
		if (paf->location == APPLY_SPELL_AFFECT)
			affect_to_char(ch, paf);
		else
			affect_modify(ch, paf, TRUE);

	if (obj->pIndexData->item_type == ITEM_LIGHT
	&&  obj->value[2] != 0
	&&  ch->in_room != NULL)
		++ch->in_room->light;

	oprog_call(OPROG_WEAR, obj, ch, NULL);
	return obj;
}

void strip_obj_affects(CHAR_DATA *ch, OBJ_DATA *obj, AFFECT_DATA *paf)
{
	AFFECT_DATA *lpaf_next = NULL;
	AFFECT_DATA *lpaf = NULL;

	for (; paf != NULL; paf = paf->next) {
		if (paf->location == APPLY_SPELL_AFFECT) {
		        for (lpaf = ch->affected; lpaf != NULL; lpaf = lpaf_next) {
				lpaf_next = lpaf->next;
				if ((lpaf->type == paf->type)
				&&  (lpaf->level == paf->level)
				&&  (lpaf->location == APPLY_SPELL_AFFECT)) {
					affect_remove(ch, lpaf);
					lpaf_next = NULL;
				}
		        }
		}
		else {
			affect_modify(ch, paf, FALSE);
			affect_check(ch,paf->where,paf->bitvector);
		}
	}
}


/*
 * Unequip a char with an obj.
 */
void unequip_char(CHAR_DATA *ch, OBJ_DATA *obj)
{
	int i;

	if (obj->wear_loc == WEAR_NONE) {
		bug("Unequip_char: already unequipped.", 0);
		return;
	}

	if (obj->wear_loc == WEAR_STUCK_IN) {
		obj->wear_loc = WEAR_NONE;
		return;
	}

	for (i = 0; i < 4; i++)
		ch->armor[i]	+= apply_ac(obj, obj->wear_loc,i);
	obj->wear_loc	 = -1;

	if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED))
		strip_obj_affects(ch, obj, obj->pIndexData->affected);
	strip_obj_affects(ch, obj, obj->affected);

	if (obj->pIndexData->item_type == ITEM_LIGHT
	&&   obj->value[2] != 0
	&&   ch->in_room != NULL
	&&   ch->in_room->light > 0)
		--ch->in_room->light;

	oprog_call(OPROG_REMOVE, obj, ch, NULL);
}

/*
 * Count occurrences of an obj in a list.
 */
int count_obj_list(OBJ_INDEX_DATA *pObjIndex, OBJ_DATA *list)
{
	OBJ_DATA *obj;
	int nMatch;

	nMatch = 0;
	for (obj = list; obj != NULL; obj = obj->next_content)
	{
		if (obj->pIndexData == pObjIndex)
		    nMatch++;
	}

	return nMatch;
}

/*
 * Move an obj out of a room.
 */
void obj_from_room(OBJ_DATA *obj)
{
	ROOM_INDEX_DATA *in_room;
	CHAR_DATA *ch;

	if ((in_room = obj->in_room) == NULL) {
		bug("obj_from_room: NULL.", 0);
		return;
	}

	for (ch = in_room->people; ch != NULL; ch = ch->next_in_room)
		if (ch->on == obj)
			ch->on = NULL;

	if (obj == in_room->contents)
		in_room->contents = obj->next_content;
	else {
		OBJ_DATA *prev;

		for (prev = in_room->contents; prev; prev = prev->next_content) {
			if (prev->next_content == obj) {
				prev->next_content = obj->next_content;
				break;
			}
		}

		if (prev == NULL) {
			bug("Obj_from_room: obj not found.", 0);
			return;
		}
	}

	obj->in_room      = NULL;
	obj->next_content = NULL;
	return;
}

/*
 * Move an obj into a room.
 */
void obj_to_room(OBJ_DATA *obj, ROOM_INDEX_DATA *pRoomIndex)
{
	obj->next_content	= pRoomIndex->contents;
	pRoomIndex->contents	= obj;
	obj->in_room		= pRoomIndex;
	obj->carried_by		= NULL;
	obj->in_obj		= NULL;

	if (IS_WATER(pRoomIndex))
		if (may_float(obj))
			obj->water_float = -1;
		else
			obj->water_float = floating_time(obj);
}

/*
 * Move an object into an object.
 */
void obj_to_obj(OBJ_DATA *obj, OBJ_DATA *obj_to)
{
	if (obj == obj_to) {
		log_printf("obj_to_obj: obj == obj_to (vnum %d)",
			   obj->pIndexData->vnum);
		return;
	}

	obj->next_content	= obj_to->contains;
	obj_to->contains	= obj;
	obj->in_obj		= obj_to;
	obj->in_room		= NULL;
	obj->carried_by		= NULL;
	if (IS_SET(obj_to->pIndexData->extra_flags, ITEM_PIT))
		obj->cost = 0; 

	for (; obj_to != NULL; obj_to = obj_to->in_obj) {
		if (obj_to->carried_by != NULL)
		{
/*	    obj_to->carried_by->carry_number += get_obj_number(obj); */
		    obj_to->carried_by->carry_weight += get_obj_weight(obj)
			* WEIGHT_MULT(obj_to) / 100;
		}
	}
}

/*
 * Move an object out of an object.
 */
void obj_from_obj(OBJ_DATA *obj)
{
	OBJ_DATA *obj_from;

	if ((obj_from = obj->in_obj) == NULL) {
		bug("Obj_from_obj: null obj_from.", 0);
		return;
	}

	if (obj == obj_from->contains)
		obj_from->contains = obj->next_content;
	else {
		OBJ_DATA *prev;

		for (prev = obj_from->contains; prev; prev = prev->next_content) {
			if (prev->next_content == obj) {
				prev->next_content = obj->next_content;
				break;
			}
		}

		if (prev == NULL) {
			bug("Obj_from_obj: obj not found.", 0);
			return;
		}
	}

	obj->next_content = NULL;
	obj->in_obj       = NULL;

	for (; obj_from != NULL; obj_from = obj_from->in_obj) {
		if (obj_from->carried_by != NULL)
/*	    obj_from->carried_by->carry_number -= get_obj_number(obj); */
			obj_from->carried_by->carry_weight -= get_obj_weight(obj) * WEIGHT_MULT(obj_from) / 100;
	}
}

/*
 * Extract an obj from the world.
 */
void extract_obj(OBJ_DATA *obj, int flags)
{
	OBJ_DATA *obj_content;
	OBJ_DATA *obj_next;

	if (obj->extracted) {
		log_printf("extract_obj: %s, vnum %d: already extracted",
			   obj->name, obj->pIndexData->vnum);
		return;
	}
	else
		obj->extracted = TRUE;
	if (IS_SET(obj->extra_flags, ITEM_CLAN)) return;
 
	for (obj_content = obj->contains; obj_content; obj_content = obj_next) {
		obj_next = obj_content->next_content;

		if (!IS_SET(flags, XO_F_NORECURSE)) {
			extract_obj(obj_content, flags);
			continue;
		}

		obj_from_obj(obj_content);
		if (obj->in_room)
			obj_to_room(obj_content, obj->in_room);
		else if (obj->carried_by)
			obj_to_char(obj_content, obj->carried_by);
		else if (obj->in_obj)
			obj_to_obj(obj_content, obj->in_obj);
		else
			extract_obj(obj_content, 0);
	}

	if (obj->in_room)
		obj_from_room(obj);
	else if (obj->carried_by)
		obj_from_char(obj);
	else if (obj->in_obj)
		obj_from_obj(obj);

	if (obj->pIndexData->vnum == OBJ_VNUM_MAGIC_JAR) {
		 CHAR_DATA *wch;
		 
		 for (wch = char_list; wch && !IS_NPC(wch); wch = wch->next) {
		 	if (is_name(obj->name, wch->name)) {
				REMOVE_BIT(wch->plr_flags, PLR_NOEXP);
				char_puts("Now you catch your spirit.\n", wch);
				break;
			}
		}
	}

	if (object_list == obj)
		object_list = obj->next;
	else {
		OBJ_DATA *prev;

		for (prev = object_list; prev != NULL; prev = prev->next)
		{
		    if (prev->next == obj)
		    {
			prev->next = obj->next;
			break;
		    }
		}

		if (prev == NULL)
		{
		    bug("Extract_obj: obj %d not found.", obj->pIndexData->vnum);
		    return;
		}
	}

	if (!IS_SET(flags, XO_F_NOCOUNT))
		--obj->pIndexData->count;
	free_obj(obj);
}

/*
 * Extract a char from the world.
 */
void extract_char(CHAR_DATA *ch, int flags)
{
	CHAR_DATA *wch;
	OBJ_DATA *obj;
	OBJ_DATA *obj_next;
	OBJ_DATA *wield;
	int extract_obj_flags;

	if (!IS_SET(flags, XC_F_INCOMPLETE)) {
		/*
		 * only for total extractions should it check
		 */
		if (ch->extracted) {
			/*
			 * if it's already been extracted,
			 * something bad is going on
			 */
			log_printf("Warning! Extraction of %s.", ch->name);
			return;
		}
		else {
			/*
			 * if it hasn't been extracted yet, now
	                 * it's being extracted.
			 */
			ch->extracted = TRUE;
		}
	}

	nuke_pets(ch);

	if (!IS_SET(flags, XC_F_INCOMPLETE))
		die_follower(ch);
	
	stop_fighting(ch, TRUE);

	if ((wield = get_eq_char(ch, WEAR_WIELD)) != NULL)
		unequip_char(ch, wield); 

	extract_obj_flags = (IS_SET(flags, XC_F_COUNT) ? 0 : XO_F_NOCOUNT);
	for (obj = ch->carrying; obj != NULL; obj = obj_next) {
		obj_next = obj->next_content;
		extract_obj(obj, extract_obj_flags);
	}
	
	if (ch->in_room)
		char_from_room(ch);

	if (IS_SET(flags, XC_F_INCOMPLETE)) {
		char_to_room(ch, get_altar(ch)->room);
		return;
	}

	if (IS_NPC(ch))
		--ch->pIndexData->count;

	if (ch->desc != NULL && ch->desc->original != NULL) {
		do_return(ch, str_empty);
		ch->desc = NULL;
	}

	for (wch = char_list; wch; wch = wch->next) {
		if (wch->reply == ch)
			wch->reply = NULL;
		if (wch->mprog_target == ch)
			wch->mprog_target = NULL;
	}

	if (ch == char_list) {
		char_list = ch->next;
		if (ch == char_list_lastpc)
			char_list_lastpc = NULL;
	}
	else {
		CHAR_DATA *prev;

		for (prev = char_list; prev; prev = prev->next) {
			if (prev->next == ch)
				break;
		}

		if (!prev) {
			bug("Extract_char: char not found.", 0);
			return;
		}

		prev->next = ch->next;
		if (ch == char_list_lastpc)
			char_list_lastpc = prev;
	}

	if (ch->desc)
		ch->desc->character = NULL;
	free_char(ch);
}

/*
 * Find a char in the room.
 */
CHAR_DATA *get_char_room_raw(CHAR_DATA *ch, const char *name, uint *number,
			     ROOM_INDEX_DATA *room)
{
	CHAR_DATA *rch;
	bool ugly;

	if (!str_cmp(name, "self"))
		return ch;

	ugly = !str_cmp(name, "ugly");
	for (rch = room->people; rch; rch = rch->next_in_room) {
		CHAR_DATA *vch;

		if (!can_see(ch, rch))
			continue;

		if (ugly
		&&  *number == 1
		&&  is_affected(rch, gsn_vampire))
			return rch;

		vch = (is_affected(rch, gsn_doppelganger) &&
		       (IS_NPC(ch) || !IS_SET(ch->plr_flags, PLR_HOLYLIGHT))) ?
					rch->doppel : rch;
		if (name[0] && !is_name(name, vch->name))
			continue;

		if (!--(*number))
			return rch;
	}

	return NULL;
}

/*
 * Find a char in the room.
 */
CHAR_DATA *get_char_room(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number)
		return NULL;

	return get_char_room_raw(ch, arg, &number, ch->in_room);
}

CHAR_DATA *get_char_area(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *ach;
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number)
		return NULL;

	if ((ach = get_char_room_raw(ch, arg, &number, ch->in_room)))
		return ach;

	if (arg[0] == '\0')
		return NULL;

	for (ach = char_list; ach; ach = ach->next) { 
		if (!ach->in_room
		||  ach->in_room == ch->in_room)
			continue;

		if (ach->in_room->area != ch->in_room->area
		||  !can_see(ch, ach)
		||  !is_name(arg, ach->name))
			continue;

		if (!--number)
			return ach;
	}
	return NULL;
}

/*
 * Find a char in the world.
 */
CHAR_DATA *get_char_world(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *wch;
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number)
		return NULL;

	if ((wch = get_char_room_raw(ch, arg, &number, ch->in_room)))
		return wch;

	if (arg[0] == '\0')
		return NULL;

	for (wch = char_list; wch; wch = wch->next) {
		if (!wch->in_room
		||  wch->in_room == ch->in_room
		||  !can_see(ch, wch) 
		||  !is_name(arg, wch->name))
			continue;

		if (!--number)
			return wch;
	}

	return NULL;
}

int opposite_door(int door)
{
	int opdoor;

	switch (door) {
	case 0: opdoor=2;	break;
	case 1: opdoor=3;	break;
	case 2: opdoor=0;	break;
	case 3: opdoor=1;	break;
	case 4: opdoor=5;	break;
	case 5: opdoor=4;	break;
	default: opdoor=-1;	break;
	}

	return opdoor;
}

CHAR_DATA *find_char(CHAR_DATA *ch, const char *argument, int door, int range) 
{
	EXIT_DATA *pExit, *bExit;
	ROOM_INDEX_DATA *dest_room = ch->in_room;
	ROOM_INDEX_DATA *back_room;
	CHAR_DATA *target;
	uint number;
	int opdoor;
	char arg[MAX_INPUT_LENGTH];

	number = number_argument(argument, arg, sizeof(arg));
	if (!number)
		return NULL;

	if ((target = get_char_room_raw(ch, arg, &number, dest_room)))
		return target;

	if ((opdoor = opposite_door(door)) == -1) {
		bug("In find_char wrong door: %d", door);
		char_puts("You don't see that there.\n", ch);
		return NULL;
	}

	while (range > 0) {
		range--;

		/* find target room */
		back_room = dest_room;
		if ((pExit = dest_room->exit[door]) == NULL
		||  (dest_room = pExit->to_room.r) == NULL
		||  IS_SET(pExit->exit_info, EX_CLOSED))
			break;

		if ((bExit = dest_room->exit[opdoor]) == NULL
		||  bExit->to_room.r != back_room) {
			char_puts("The path you choose prevents your power "
				  "to pass.\n",ch);
			return NULL;
		}
		if ((target = get_char_room_raw(ch, arg, &number, dest_room))) 
			return target;
	}

	char_puts("You don't see that there.\n", ch);
	return NULL;
}

int check_exit(char *arg)
{
	int door = -1;

	     if (!str_cmp(arg, "n") || !str_cmp(arg, "north")) door = 0;
	else if (!str_cmp(arg, "e") || !str_cmp(arg, "east" )) door = 1;
	else if (!str_cmp(arg, "s") || !str_cmp(arg, "south")) door = 2;
	else if (!str_cmp(arg, "w") || !str_cmp(arg, "west" )) door = 3;
	else if (!str_cmp(arg, "u") || !str_cmp(arg, "up"   )) door = 4;
	else if (!str_cmp(arg, "d") || !str_cmp(arg, "down" )) door = 5;

	return door;
}

/*
 * Find a char for range casting.
 * argument must specify target in form '[d.][n.]name' where
 * 'd' - direction
 * 'n' - number
 */
CHAR_DATA *get_char_spell(CHAR_DATA *ch, const char *argument,
			  int *door, int range)
{
	char buf[MAX_INPUT_LENGTH];
	char *p;

	p = strchr(argument, '.');
	if (!p) {
		*door = -1;
		return get_char_room(ch, argument);
	}

	strnzncpy(buf, sizeof(buf), argument, p-argument);
	if ((*door = check_exit(buf)) < 0)
		return get_char_room(ch, argument);

	return find_char(ch, p+1, *door, range);
}

/*
 * Find some object with a given index data.
 * Used by area-reset 'P' command.
 */
OBJ_DATA *get_obj_type(OBJ_INDEX_DATA *pObjIndex)
{
	OBJ_DATA *obj;

	for (obj = object_list; obj; obj = obj->next)
		if (obj->pIndexData == pObjIndex)
			return obj;

	return NULL;
}

/*
 * flags for get_obj_list_raw
 */
enum {
	GETOBJ_F_WEAR_ANY,	/* any obj->wear_loc			     */
	GETOBJ_F_WEAR_NONE,	/* obj->wear_loc == WEAR_NONE (in inventory) */
	GETOBJ_F_WEAR,		/* obj->wear_loc != WEAR_NONE (worn)	     */
};

/*
 * Find an obj in a list.
 */
OBJ_DATA *get_obj_list_raw(CHAR_DATA *ch, const char *name, uint *number,
			   OBJ_DATA *list, int flags)
{
	OBJ_DATA *obj;

	for (obj = list; obj; obj = obj->next_content) {
		if (!can_see_obj(ch, obj)
		||  !is_name(name, obj->name))
			continue;

		switch (flags) {
		case GETOBJ_F_WEAR_NONE:
			if (obj->wear_loc != WEAR_NONE)
				continue;
			break;

		case GETOBJ_F_WEAR:
			if (obj->wear_loc == WEAR_NONE)
				continue;
			break;
		}

		if (!--(*number))
			return obj;
	}

	return NULL;
}

/*
 * Find an obj in the room or in eq/inventory.
 */
OBJ_DATA *get_obj_here_raw(CHAR_DATA *ch, const char *name, uint *number)
{
	OBJ_DATA *obj;

/* search in player's inventory */
	obj = get_obj_list_raw(ch, name, number, ch->carrying,
			       GETOBJ_F_WEAR_NONE);
	if (obj)
		return obj;

/* search in player's eq */
	obj = get_obj_list_raw(ch, name, number, ch->carrying, GETOBJ_F_WEAR);
	if (obj)
		return obj;

/* search in room contents */
	obj = get_obj_list_raw(ch, name, number, ch->in_room->contents,
			       GETOBJ_F_WEAR_ANY);
	if (obj)
		return obj;

	return NULL;
}

/*
 * Find an obj in a list.
 */
OBJ_DATA *get_obj_list(CHAR_DATA *ch, const char *argument, OBJ_DATA *list)
{
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	return get_obj_list_raw(ch, arg, &number, list, GETOBJ_F_WEAR_ANY);
}

/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *get_obj_carry(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	return get_obj_list_raw(ch, arg, &number, ch->carrying,
				GETOBJ_F_WEAR_NONE);
}

/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *get_obj_wear(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	return get_obj_list_raw(ch, arg, &number, ch->carrying, GETOBJ_F_WEAR);
}

/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	return get_obj_here_raw(ch, arg, &number);
}

OBJ_DATA *get_obj_room(CHAR_DATA *ch, const char *argument)
{
	OBJ_DATA *obj;
	CHAR_DATA *vch;
	char arg[MAX_INPUT_LENGTH];
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	if ((obj = get_obj_here_raw(ch, arg, &number)))
		return obj;

	for (vch = ch->in_room->people; vch; vch = vch->next_in_room) {
		/*
		 * search in the vch's inventory
		 */
		obj = get_obj_list_raw(ch, arg, &number, vch->carrying,
				       GETOBJ_F_WEAR_NONE);
		if (obj)
			return obj;

		/*
		 * search in the vch's eq
		 */
		obj = get_obj_list_raw(ch, arg, &number, vch->carrying,
				       GETOBJ_F_WEAR);
		if (obj)
			return obj;
	}

	return NULL;
}

/*
 * Find an obj in the world.
 */
OBJ_DATA *get_obj_world(CHAR_DATA *ch, const char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	OBJ_DATA *obj;
	uint number;

	number = number_argument(argument, arg, sizeof(arg));
	if (!number || arg[0] == '\0')
		return NULL;

	if ((obj = get_obj_here_raw(ch, arg, &number)))
		return obj;

	for (obj = object_list; obj; obj = obj->next)
		if (can_see_obj(ch, obj)
		&&  obj->carried_by != ch
		&&  is_name(arg, obj->name)
		&&  !--number)
			return obj;

	return NULL;
}

/*
 * deduct cost from a character
 */
void deduct_cost(CHAR_DATA *ch, uint cost)
{
	/*
	 * price in silver. MUST BE signed for proper exchange operations
	 */
	int silver = UMIN(ch->silver, cost); 
	int gold = 0;

	if (silver < cost) {
		gold = (cost - silver + 99) / 100;
		silver = cost - 100 * gold;
	}

	if (ch->gold < gold) {
		log_printf("deduct cost: %s: ch->gold (%d) < gold (%d)",
			   ch->name, ch->gold, gold);
		ch->gold = gold;
	}

	if (ch->silver < silver) {
		log_printf("deduct cost: %s: ch->silver (%d) < silver (%d)",
			   ch->name, ch->silver, silver);
		ch->silver = silver;
	}

	ch->gold -= gold;
	ch->silver -= silver;
} 

static inline void
money_form(int lang, char *buf, size_t len, int num, const char *name)
{
	char tmp[MAX_STRING_LENGTH];

	if (num < 0)
		return;

	strnzcpy(tmp, sizeof(tmp),
		 word_form(GETMSG(name, lang), 1, lang, RULES_CASE));
	strnzcpy(buf, len, word_form(tmp, num, lang, RULES_QTY));
}

struct _data {
	int num1;
	const char *name1;
	int num2;
	const char *name2;
};

static void
money_cb(int lang, const char **p, void *arg)
{
	char buf1[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH];

	const char *q;
	struct _data *d = (struct _data *) arg;

	if (IS_NULLSTR(*p))
		return;

	money_form(lang, buf1, sizeof(buf1), d->num1, d->name1);
	money_form(lang, buf2, sizeof(buf2), d->num2, d->name2);

	q = str_printf(*p, d->num1, buf1, d->num2, buf2);
	free_string(*p);
	*p = q;
}

static void
money_descr(mlstring **descr,
	    int num1, const char *name1,
	    int num2, const char *name2)
{
	struct _data data;

	data.num1 = num1;
	data.num2 = num2;
	data.name1 = name1;
	data.name2 = name2;

	mlstr_for_each(descr, &data, money_cb);
}

/*
 * Create a 'money' obj.
 */
OBJ_DATA *create_money(int gold, int silver)
{
	OBJ_INDEX_DATA *pObjIndex;
	OBJ_DATA *obj;

	if (gold < 0 || silver < 0 || (gold == 0 && silver == 0)) {
		log_printf("create_money: gold %d, silver %d",
			   gold, silver);
		gold = UMAX(1, gold);
		silver = UMAX(1, silver);
	}

	if (gold == 0 && silver == 1)
		obj = create_obj(get_obj_index(OBJ_VNUM_SILVER_ONE), 0);
	else if (gold == 1 && silver == 0)
		obj = create_obj(get_obj_index(OBJ_VNUM_GOLD_ONE), 0);
	else if (silver == 0) {
		pObjIndex = get_obj_index(OBJ_VNUM_GOLD_SOME);
		obj = create_obj(pObjIndex, 0);
		money_descr(&obj->short_descr, gold, "gold coins", -1, NULL);
		obj->value[1]	= gold;
		obj->cost	= 100*gold;
		obj->weight	= gold/5;
	}
	else if (gold == 0) {
		pObjIndex = get_obj_index(OBJ_VNUM_SILVER_SOME);
		obj = create_obj(pObjIndex, 0);
		money_descr(&obj->short_descr,
			    silver, "silver coins", -1, NULL);
		obj->value[0]	= silver;
		obj->cost	= silver;
		obj->weight	= silver/20;
	}
	else {
		pObjIndex = get_obj_index(OBJ_VNUM_COINS);
		obj = create_obj(pObjIndex, 0);
		money_descr(&obj->short_descr,
			    silver, "silver coins", gold, "gold coins");
		obj->value[0]	= silver;
		obj->value[1]	= gold;
		obj->cost	= 100*gold + silver;
		obj->weight	= gold/5 + silver/20;
	}

	return obj;
}


/*
 * Return # of objects which an object counts as.
 * Thanks to Tony Chamberlain for the correct recursive code here.
 */
int get_obj_number(OBJ_DATA *obj)
{
	int number;
/* 
	if (obj->pIndexData->item_type == ITEM_CONTAINER || obj->pIndexData->item_type == ITEM_MONEY
	||  obj->pIndexData->item_type == ITEM_GEM || obj->pIndexData->item_type == ITEM_JEWELRY)
	    number = 0;
*/
	if (obj->pIndexData->item_type == ITEM_MONEY)
		number = 0;
	else
	    number = 1;

/* 
	for (obj = obj->contains; obj != NULL; obj = obj->next_content)
	    number += get_obj_number(obj);
*/ 
	return number;
}

int get_obj_realnumber(OBJ_DATA *obj)
{
	int number = 1;

	for (obj = obj->contains; obj != NULL; obj = obj->next_content)
	    number += get_obj_number(obj);

	return number;
}

/*
 * Return weight of an object, including weight of contents.
 */
int get_obj_weight(OBJ_DATA *obj)
{
	int weight;
	OBJ_DATA *tobj;

	weight = obj->weight;
	for (tobj = obj->contains; tobj != NULL; tobj = tobj->next_content)
		weight += get_obj_weight(tobj) * WEIGHT_MULT(obj) / 100;

	return weight;
}

int get_true_weight(OBJ_DATA *obj)
{
	int weight;
 
	weight = obj->weight;
	for (obj = obj->contains; obj != NULL; obj = obj->next_content)
	    weight += get_obj_weight(obj);
 
	return weight;
}

/*
 * True if room is dark.
 */
bool room_is_dark(CHAR_DATA *ch)
{
	ROOM_INDEX_DATA * pRoomIndex = ch->in_room;

	if (!IS_NPC(ch) && IS_SET(ch->plr_flags, PLR_HOLYLIGHT))
		return FALSE;

	if (is_affected(ch, gsn_vampire))
		return FALSE;
		
	if (pRoomIndex->light > 0)
		return FALSE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_DARK))
		return TRUE;

	if (pRoomIndex->sector_type == SECT_INSIDE
	||   pRoomIndex->sector_type == SECT_CITY)
		return FALSE;

	if (weather_info.sunlight == SUN_SET
	||  weather_info.sunlight == SUN_DARK)
		return TRUE;

	return FALSE;
}

bool room_dark(ROOM_INDEX_DATA *pRoomIndex)
{
	if (pRoomIndex->light > 0)
		return FALSE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_DARK))
		return TRUE;

	if (pRoomIndex->sector_type == SECT_INSIDE
	||   pRoomIndex->sector_type == SECT_CITY)
		return FALSE;

	if (weather_info.sunlight == SUN_SET
		   || weather_info.sunlight == SUN_DARK)
		return TRUE;

	return FALSE;
}


bool is_room_owner(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
	if (room->owner == NULL || room->owner[0] == '\0')
		return FALSE;

	return is_name(ch->name,room->owner);
}

/*
 * True if room is private.
 */
bool room_is_private(ROOM_INDEX_DATA *pRoomIndex)
{
	CHAR_DATA *rch;
	int count;

/*
	if (pRoomIndex->owner != NULL && pRoomIndex->owner[0] != '\0')
		return TRUE;
*/
	count = 0;
	for (rch = pRoomIndex->people; rch != NULL; rch = rch->next_in_room)
		count++;

	if (IS_SET(pRoomIndex->room_flags, ROOM_PRIVATE)  && count >= 2)
		return TRUE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_SOLITARY) && count >= 1)
		return TRUE;
	
	if (IS_SET(pRoomIndex->room_flags, ROOM_IMP_ONLY))
		return TRUE;

	return FALSE;
}

/* visibility on a room -- for entering and exits */
bool can_see_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
	if (IS_SET(pRoomIndex->room_flags, ROOM_IMP_ONLY) 
	&&  !IS_TRUSTED(ch, IMPLEMENTOR))
		return FALSE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_GODS_ONLY)
	&&  !IS_TRUSTED(ch, GOD))
		return FALSE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_HEROES_ONLY)
	&&  !IS_TRUSTED(ch, HERO))
		return FALSE;

	if (IS_SET(pRoomIndex->room_flags, ROOM_NEWBIES_ONLY)
	&&  ch->level > LEVEL_NEWBIE && !IS_IMMORTAL(ch))
		return FALSE;

	return TRUE;
}

/*
 * True if char can see victim.
 */
bool can_see(CHAR_DATA *ch, CHAR_DATA *victim)
{
/* RT changed so that WIZ_INVIS has levels */
	if (ch == victim)
		return TRUE;

	if (ch == NULL || victim == NULL) {
		log("can_see: NULL ch or victim");
		return FALSE;
	}
	
	if (!IS_NPC(victim) && !IS_TRUSTED(ch, victim->invis_level))
		return FALSE;

	if (IS_CLAN_GUARD(ch)) return TRUE;

	if (!IS_TRUSTED(ch, victim->incog_level)
	&&  ch->in_room != victim->in_room)
		return FALSE;

	if (!IS_NPC(ch) && IS_SET(ch->plr_flags, PLR_HOLYLIGHT))
		return TRUE;

	if (IS_NPC(victim) && !IS_TRUSTED(ch, victim->invis_level)) {
		AREA_DATA *pArea = area_vnum_lookup(victim->pIndexData->vnum);
		if (pArea == NULL
		||  !IS_BUILDER(ch, pArea))
			return FALSE;
	}

	if (IS_AFFECTED(ch, AFF_BLIND))
		return FALSE;

	if (ch->in_room == NULL)
	    return FALSE;

	if (room_is_dark(ch) && !IS_AFFECTED(ch, AFF_INFRARED))
		return FALSE;

	if (IS_AFFECTED(victim, AFF_INVIS)
	&&   !IS_AFFECTED(ch, AFF_DETECT_INVIS))
		return FALSE;

	if (IS_AFFECTED(victim, AFF_IMP_INVIS)
	&&   !IS_AFFECTED(ch, AFF_DETECT_IMP_INVIS))
		return FALSE;

	if (IS_AFFECTED(victim,AFF_CAMOUFLAGE) &&
	    !IS_AFFECTED(ch,AFF_ACUTE_VISION))
	  return FALSE;

	if (IS_AFFECTED(victim, AFF_HIDE)
	&&   !IS_AFFECTED(ch, AFF_DETECT_HIDDEN)
	&&   victim->fighting == NULL)
		return FALSE;

	if (IS_AFFECTED(victim, AFF_FADE)
	&&   !IS_AFFECTED(ch, AFF_DETECT_FADE)
	&&   victim->fighting == NULL)
		return FALSE;

	return TRUE;
}

/*
 * True if char can see obj.
 */
bool can_see_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
	if (!IS_NPC(ch) && IS_SET(ch->plr_flags, PLR_HOLYLIGHT))
		return TRUE;

	if (IS_SET(obj->extra_flags, ITEM_VIS_DEATH))
		return FALSE;

	if (IS_AFFECTED(ch, AFF_BLIND) && obj->pIndexData->item_type != ITEM_POTION)
		return FALSE;

	if (obj->pIndexData->item_type == ITEM_LIGHT && obj->value[2] != 0)
		return TRUE;

	if (IS_SET(obj->extra_flags, ITEM_INVIS)
	&&   !IS_AFFECTED(ch, AFF_DETECT_INVIS))
	    return FALSE;

	if (IS_OBJ_STAT(obj, ITEM_GLOW))
		return TRUE;

	if (room_is_dark(ch) && !IS_AFFECTED(ch, AFF_INFRARED))
		return FALSE;

	return TRUE;
}

/*
 * True if char can drop obj.
 */
bool can_drop_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
	if (!IS_SET(obj->extra_flags, ITEM_NODROP))
		return TRUE;

	if (IS_IMMORTAL(ch))
		return TRUE;

	return FALSE;
}

int isn_dark_safe(CHAR_DATA *ch)
{
	CHAR_DATA *rch;
	OBJ_DATA *light;
	int light_exist;

	if (!is_affected(ch, gsn_vampire)
	||  IS_SET(ch->in_room->room_flags, ROOM_DARK))
		return 0;

	if (weather_info.sunlight == SUN_LIGHT
	||  weather_info.sunlight == SUN_RISE)
		return 2;

	light_exist = 0;
	for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
		if ((light = get_eq_char(rch, WEAR_LIGHT))
		&&  IS_OBJ_STAT(light, ITEM_MAGIC)) {
			light_exist = 1;
			break;
		}
	}

	return light_exist;
}

int count_charmed(CHAR_DATA *ch)	
{
  CHAR_DATA *gch;
  int count = 0;

  for (gch = char_list; gch != NULL; gch = gch->next)
	{
	  if (IS_AFFECTED(gch,AFF_CHARM) && gch->master == ch)
		  count++;
	}

  if (count >= MAX_CHARM(ch))
   {
	char_puts("You are already controlling as many charmed mobs as you can!\n",ch);
	return count;
   }
  return 0;
}

/*
 * add_mind - remember 'str' in mind buffer of 'ch'
 *	      remember the place to return in mind buffer
 */
void add_mind(CHAR_DATA *ch, const char *str)
{
	if (!IS_NPC(ch) || ch->in_room == NULL)
		return;

	if (ch->in_mind == NULL)
		/* remember a place to return */
		ch->in_mind = str_printf("%d", ch->in_room->vnum);

	if (!is_name(str, ch->in_mind)) {
		const char *p = ch->in_mind;
		ch->in_mind = str_printf("%s %s", ch->in_mind, str);
		free_string(p);
	}
}

/*
 * remove_mind - remove 'str' from mind buffer of 'ch'
 *		 if it was the last revenge - return home
 */
void remove_mind(CHAR_DATA *ch, const char *str)
{
	char buf[MAX_STRING_LENGTH];
	char buff[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	const char *mind = ch->in_mind;

	if (!IS_NPC(ch) || ch->in_room == NULL 
	||  mind == NULL || !is_name(str, mind)) return;

	buf[0] = '\0';
	do { 
		mind = one_argument(mind, arg, sizeof(arg));
		if (!is_name(str,arg))  {
			if (buf[0] == '\0')
				strnzcpy(buf, sizeof(buf), arg);
			else {
				snprintf(buff, sizeof(buff), "%s %s", buf, arg);
				strnzcpy(buf, sizeof(buf), buff);
			}
		}
	}
	while (mind[0] != '\0');
 
	free_string(ch->in_mind);
	if (is_number(buf)) {
		do_say(ch, "At last, I took my revenge!"); 
		back_home(ch);
		ch->in_mind = NULL;
	}
	else
		ch->in_mind = str_dup(buf);
}

void back_home(CHAR_DATA *ch)
{
	ROOM_INDEX_DATA *location;
	char arg[MAX_INPUT_LENGTH];

	if (!IS_NPC(ch) || ch->in_mind == NULL)
		return;

	one_argument(ch->in_mind, arg, sizeof(arg));
	if ((location = find_location(ch, arg)) == NULL) {
		log("back_home: reset place not found");
		return;
	}

	if (ch->fighting == NULL && location != ch->in_room) {
		act("$n prays for transportation.", ch, NULL, NULL, TO_ROOM);
		recall(ch, location);
	}
}

void path_to_track(CHAR_DATA *ch, CHAR_DATA *victim, int door)
{
  ROOM_INDEX_DATA *temp;
  EXIT_DATA *pExit;
  int opdoor;
  int range = 0;

	SET_FIGHT_TIME(ch);
  	if (!IS_NPC(victim))
		SET_FIGHT_TIME(victim);

  if (IS_NPC(victim) && victim->position != POS_DEAD)
   {
	victim->last_fought = ch;

	if ((opdoor = opposite_door(door)) == -1)
		{
		 bug("In path_to_track wrong door: %d",door);
		 return;
		}
	temp = ch->in_room;
	while (1)
	 {
	  range++;
	  if (victim->in_room == temp) break;
	  if ((pExit = temp->exit[ door ]) == NULL
		  || (temp = pExit->to_room.r) == NULL)
	   {
		bug("In path_to_track: couldn't calculate range %d",range);
		return;
	   }
	  if (range > 100)
	   {
		bug("In path_to_track: range exceeded 100",0);
		return;
	   }
	 }

	temp = victim->in_room;
	while (--range > 0)
	   {
	    room_record(ch->name,temp, opdoor);
	    if ((pExit = temp->exit[opdoor]) == NULL
		    || (temp = pExit->to_room.r) == NULL)
		{
		 log_printf("[*****] Path to track: Range: %d Room: %d opdoor:%d",
			range,temp->vnum,opdoor); 
		 return;
		}
	   }
	do_track(victim,str_empty);
  }
}

int pk_range(int level)
{
	return UMAX(4, level/10 + 2);
}

bool in_PK(CHAR_DATA *ch, CHAR_DATA *victim)
{
	if (IS_NPC(ch) || IS_NPC(victim))
		return TRUE;

	if (victim->level < MIN_PK_LEVEL || ch->level < MIN_PK_LEVEL)
		return FALSE;

	/* level adjustment */
	if (ch != victim && !IS_IMMORTAL(ch)
	&&  (ch->level >= (victim->level + pk_range(ch->level)) ||
	     ch->level <= (victim->level - pk_range(ch->level)))
	&&  (victim->level >= (ch->level + pk_range(victim->level)) ||
	     victim->level <= (ch->level - pk_range(victim->level))))
		return FALSE;

	return TRUE;
}

bool can_gate(CHAR_DATA *ch, CHAR_DATA *victim)
{
	if (victim == ch
	||  ch->fighting != NULL
	||  victim->in_room == NULL
	||  !can_see_room(ch, victim->in_room)
	||  IS_SET(ch->in_room->room_flags, ROOM_SAFE | ROOM_NORECALL |
					    ROOM_PEACE | ROOM_NOSUMMON)
	||  IS_SET(victim->in_room->room_flags, ROOM_SAFE | ROOM_NORECALL |
						ROOM_PEACE | ROOM_NOSUMMON)
	||  IS_SET(ch->in_room->area->flags, AREA_CLOSED)
	||  room_is_private(victim->in_room)
	||  IS_SET(victim->imm_flags, IMM_SUMMON))
		return FALSE;

	if (IS_NPC(victim))
		return TRUE;

	if (((!in_PK(ch, victim) ||
	      ch->in_room->area != victim->in_room->area) &&
	     IS_SET(victim->plr_flags, PLR_NOSUMMON))
	||  victim->level >= LEVEL_HERO
	||  !guild_ok(ch, victim->in_room))
		return FALSE;

	return TRUE;
}

void transfer_char(CHAR_DATA *ch, CHAR_DATA *vch, ROOM_INDEX_DATA *to_room,
		   const char *msg_out,
		   const char *msg_travel,
		   const char *msg_in)
{
	ROOM_INDEX_DATA *was_in = ch->in_room;

	if (ch != vch)
		act_puts(msg_travel, vch, NULL, ch, TO_VICT, POS_DEAD);

	char_from_room(ch);

	act(msg_out, was_in->people, NULL, ch, TO_ALL);
	act(msg_in, to_room->people, NULL, ch, TO_ALL);

	char_to_room(ch, to_room);

	if (!JUST_KILLED(ch))
		do_look(ch, "auto");
}

void
recall(CHAR_DATA *ch, ROOM_INDEX_DATA *location)
{
	transfer_char(ch, NULL, location,
		      "$N disappears.", NULL, "$N appears in the room.");
}

void look_at(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
	ROOM_INDEX_DATA *was_in = ch->in_room;
	OBJ_DATA *obj;
	bool adjust_light = FALSE;

	if ((obj = get_eq_char(ch, WEAR_LIGHT))
	&&  obj->pIndexData->item_type == ITEM_LIGHT
	&&  obj->value[2]) {
		adjust_light = TRUE;
		room->light++;
	}
		
	ch->in_room = room;
	do_look(ch, str_empty);
	ch->in_room = was_in;

	if (adjust_light)
		room->light--;
}

/* random room generation procedure */
ROOM_INDEX_DATA  *get_random_room(CHAR_DATA *ch, AREA_DATA *area)
{
	int min_vnum;
	int max_vnum;
	ROOM_INDEX_DATA *room;

	if (!area) {
		min_vnum = 1;
		max_vnum = top_vnum_room;
	}
	else {
		min_vnum = area->min_vnum;
		max_vnum = area->max_vnum;
	}

	for (; ;) {
		room = get_room_index(number_range(min_vnum, max_vnum));

		if (!room)
			continue;

		if (can_see_room(ch, room)
		&&  !room_is_private(room)
		&&  !IS_SET(room->room_flags, ROOM_SAFE | ROOM_PEACE) 
		&&  !(IS_SET(room->area->flags, AREA_CLOSED) && !area)
		&&  (!IS_NPC(ch) ||
		     !IS_SET(ch->pIndexData->act, ACT_AGGRESSIVE) ||
		     !IS_SET(room->room_flags, ROOM_LAW)))
			break;
	}

	return room;
}

const char *PERS2(CHAR_DATA *ch, CHAR_DATA *looker, flag32_t flags)
{
	if (is_affected(ch, gsn_doppelganger)
	&&  (IS_NPC(looker) || !IS_SET(looker->plr_flags, PLR_HOLYLIGHT)))
		ch = ch->doppel;

	if (can_see(looker, ch)) {
		if (IS_NPC(ch)) {
			const char *descr;

			if (IS_SET(flags, ACT_FORMSH)) {
				return format_short(ch->short_descr, ch->name,
						    looker);
			}

			descr = mlstr_cval(ch->short_descr, looker);
			if (IS_SET(flags, ACT_FIXSH))
				return fix_short(descr);

			return descr;
		}
		else if (is_affected(ch, gsn_vampire) && !IS_IMMORTAL(looker)) {
			return word_form(GETMSG("an ugly creature",
						looker->lang),
					 ch->sex, looker->lang, RULES_GENDER);
		}
		return ch->name;
	}

	if (IS_IMMORTAL(ch)) {
		return word_form(GETMSG("an immortal", looker->lang), ch->sex,
				 looker->lang, RULES_GENDER);
	}

	return "someone";
}

void format_obj(BUFFER *output, OBJ_DATA *obj)
{
	buf_printf(output,
		"Object '%s' is type %s, extra flags %s.\n"
		"Weight is %d, value is %d, level is %d.\n",
		obj->name,
		flag_string(item_types, obj->pIndexData->item_type),
		flag_string(extra_flags, obj->extra_flags & ~ITEM_ENCHANTED),
		obj->weight / 10,
		obj->cost,
		obj->level);

	if (obj->pIndexData->limit != -1)
		buf_printf(output,
			   "This equipment has been LIMITED by number %d \n",
			   obj->pIndexData->limit);

	switch (obj->pIndexData->item_type) {
	case ITEM_SCROLL:
	case ITEM_POTION:
	case ITEM_PILL:
		buf_printf(output, "Level %d spells of:", obj->value[0]);

		if (obj->value[1] >= 0)
			buf_printf(output, " '%s'", skill_name(obj->value[1]));

		if (obj->value[2] >= 0)
			buf_printf(output, " '%s'", skill_name(obj->value[2]));

		if (obj->value[3] >= 0)
			buf_printf(output, " '%s'", skill_name(obj->value[3]));

		if (obj->value[4] >= 0)
			buf_printf(output, " '%s'", skill_name(obj->value[4]));

		buf_add(output, ".\n");
		break;

	case ITEM_WAND: 
	case ITEM_STAFF: 
		buf_printf(output, "Has %d charges of level %d",
			   obj->value[2], obj->value[0]);
	  
		if (obj->value[3] >= 0)
			buf_printf(output, " '%s'", skill_name(obj->value[3]));

		buf_add(output, ".\n");
		break;

	case ITEM_DRINK_CON:
		buf_printf(output, "It holds %s-colored %s.\n",
			   liq_table[obj->value[2]].liq_color,
	        	   liq_table[obj->value[2]].liq_name);
		break;

	case ITEM_CONTAINER:
		buf_printf(output,
			   "Capacity: %d#  Maximum weight: %d#  flags: %s\n",
			   obj->value[0], obj->value[3],
			   flag_string(cont_flags, obj->value[1]));
		if (obj->value[4] != 100)
			buf_printf(output, "Weight multiplier: %d%%\n",
				   obj->value[4]);
		break;
			
	case ITEM_WEAPON:
		buf_printf(output, "Weapon type is %s.\n",
			   flag_string(weapon_class, obj->value[0]));
		buf_printf(output, "Damage is %dd%d (average %d).\n",
			   obj->value[1],obj->value[2],
			   (1 + obj->value[2]) * obj->value[1] / 2);
		if (obj->value[4])
	        	buf_printf(output, "Weapons flags: %s\n",
				   flag_string(weapon_type2, obj->value[4]));
		break;

	case ITEM_ARMOR:
		buf_printf(output, "Armor class is %d pierce, "
				   "%d bash, %d slash, and %d vs. magic.\n", 
			   obj->value[0], obj->value[1],
			   obj->value[2], obj->value[3]);
		break;
	}
}

void format_obj_affects(BUFFER *output, AFFECT_DATA *paf, int flags)
{
	for (; paf; paf = paf->next) {
		where_t *w;

		if (paf->location != APPLY_NONE && paf->modifier) { 
			buf_printf(output, "Affects %s by %d",
				   flag_string(apply_flags, paf->location),
				   paf->modifier);
			if (!IS_SET(flags, FOA_F_NODURATION)
			&&  paf->duration > -1)
				buf_printf(output, " for %d hours",
					   paf->duration);
			buf_add(output, ".\n");
		}

		if (IS_SET(flags, FOA_F_NOAFFECTS))
			continue;

		if ((w = where_lookup(paf->where)) && paf->bitvector) {
			buf_add(output, "Adds ");
			buf_printf(output, w->format,
				   flag_string(w->table, paf->bitvector));
			buf_add(output, ".\n");
		}
	}
}

int get_wear_level(CHAR_DATA *ch, OBJ_DATA *obj)
{
	int wear_level = ch->level;
	class_t *cl;

	if ((cl = class_lookup(ch->class)) == NULL)
		return wear_level;

	switch (obj->pIndexData->item_type) {
	case ITEM_POTION:
	case ITEM_PILL:
	case ITEM_WAND:
	case ITEM_STAFF:
	case ITEM_SCROLL:
		return wear_level;
	}

	if (!IS_SET(obj->pIndexData->extra_flags, ITEM_QUEST)
	&&  (obj->pIndexData->limit < 0 || obj->pIndexData->limit > 1))
		wear_level += pk_range(wear_level);

	if (IS_SET(cl->flags, CLASS_MAGIC)) {
		if (obj->pIndexData->item_type == ITEM_ARMOR)
			wear_level += 3;
	}
	else if (obj->pIndexData->item_type == ITEM_WEAPON)
		wear_level += 3;
	return wear_level;
}

/*
 * Compute a saving throw.
 * Negative apply's make saving throw better.
 */
bool saves_spell(int level, CHAR_DATA *victim, int dam_type)
{
	class_t *vcl;
	int save;

	save = 40 + (victim->level + victim->drain_level - level) * 4 - 
		(victim->saving_throw * 90) / UMAX(45, victim->level);

	if (IS_AFFECTED(victim, AFF_BERSERK))
		save += victim->level / 5;

	switch(check_immune(victim, dam_type)) {
	case IS_IMMUNE:
		return TRUE;
	case IS_RESISTANT:
		save += victim->level / 5;
		break;
	case IS_VULNERABLE:
		save -= victim->level / 5;
		break;
	}

	if (!IS_NPC(victim) && (vcl = class_lookup(victim->class))
	&&  IS_SET(vcl->flags, CLASS_MAGIC))
		save = 9 * save / 10;
	save = URANGE(5, save, 95);
	return number_percent() < save;
}

/* RT configuration smashed */

bool saves_dispel(int dis_level, int spell_level, int duration)
{
	int save;
	
	  /* impossible to dispel permanent effects */
	if (duration == -2) return 1;
	if (duration == -1) spell_level += 5;

	save = 50 + (spell_level - dis_level) * 5;
	save = URANGE(5, save, 95);
	return number_percent() < save;
}

/* co-routine for dispel magic and cancellation */

bool check_dispel(int dis_level, CHAR_DATA *victim, int sn)
{
	AFFECT_DATA *af;

	if (is_affected(victim, sn)) {
	    for (af = victim->affected; af != NULL; af = af->next) {
	        if (af->type == sn) {
	            if (!saves_dispel(dis_level,af->level,af->duration)) {
			skill_t *sk;

	                affect_strip(victim,sn);
			if ((sk = skill_lookup(sn))
			&&  !IS_NULLSTR(sk->msg_off))
				char_printf(victim, "%s\n", sk->msg_off);
			return TRUE;
		    } else
			af->level--;
	        }
	    }
	}
	return FALSE;
}

bool check_blind_raw(CHAR_DATA *ch)
{
	if (!IS_NPC(ch) && IS_SET(ch->plr_flags, PLR_HOLYLIGHT))
		return TRUE;

	if (IS_AFFECTED(ch, AFF_BLIND))
		return FALSE;

	return TRUE;
}

bool check_blind(CHAR_DATA *ch)
{
	bool can_see = check_blind_raw(ch);

	if (!can_see)
		char_puts("You can't see a thing!\n", ch);

	return can_see;
}

/*----------------------------------------------------------------------------
 * show affects stuff
 */

void show_name(CHAR_DATA *ch, BUFFER *output,
	       AFFECT_DATA *paf, AFFECT_DATA *paf_last)
{
	if (paf_last && paf->type == paf_last->type)
		if (ch && ch->level < 20)
			return;
		else
			buf_add(output, "                      ");
	else
		buf_printf(output, "Spell: {c%-15s{x", skill_name(paf->type));
}

void show_duration(BUFFER *output, AFFECT_DATA *paf)
{
	if (paf->duration < 0)
		buf_add(output, "permanently.");
	else
		buf_printf(output, "for {c%d{x hours.", paf->duration);
	buf_add(output, "\n");
}

void show_loc_affect(CHAR_DATA *ch, BUFFER *output,
		 AFFECT_DATA *paf, AFFECT_DATA **ppaf)
{
	if (ch->level < 20) {
		show_name(ch, output, paf, *ppaf);
		if (*ppaf && (*ppaf)->type == paf->type)
			return;
		buf_add(output, "\n");
		*ppaf = paf;
		return;
	}

	if (paf->location > 0) {
		show_name(ch, output, paf, *ppaf);
		buf_printf(output, ": modifies {c%s{x by {c%d{x ",
			   flag_string(apply_flags, paf->location),
			   paf->modifier);
		show_duration(output, paf);
		*ppaf = paf;
	}
}

void show_bit_affect(BUFFER *output, AFFECT_DATA *paf, AFFECT_DATA **ppaf,
		     flag32_t where)
{
	char buf[MAX_STRING_LENGTH];
	where_t *w;

	if (paf->where != where
	||  (w = where_lookup(paf->where)) == NULL
	||  !paf->bitvector)
		return;

	show_name(NULL, output, paf, *ppaf);
	snprintf(buf, sizeof(buf), ": adds %s ", w->format);
	buf_printf(output, buf, flag_string(w->table, paf->bitvector));
	show_duration(output, paf);
	*ppaf = paf;
}

void show_obj_affects(CHAR_DATA *ch, BUFFER *output, AFFECT_DATA *paf)
{
	AFFECT_DATA *paf_last = NULL;

	for (; paf; paf = paf->next)
		if (paf->location != APPLY_SPELL_AFFECT)
			show_bit_affect(output, paf, &paf_last, TO_AFFECTS);
}

void show_affects(CHAR_DATA *ch, BUFFER *output)
{
	OBJ_DATA *obj;
	AFFECT_DATA *paf, *paf_last = NULL;

	buf_add(output, "You are affected by the following spells:\n");
	for (paf = ch->affected; paf; paf = paf->next) {
		show_loc_affect(ch, output, paf, &paf_last);
		if (ch->level < 20)
			continue;
		show_bit_affect(output, paf, &paf_last, TO_AFFECTS);
	}

	if (ch->level < 20)
		return;

	for (obj = ch->carrying; obj; obj = obj->next_content)
		if (obj->wear_loc != WEAR_NONE) {
			if (!IS_SET(obj->extra_flags, ITEM_ENCHANTED))
				show_obj_affects(ch, output,
						 obj->pIndexData->affected);
			show_obj_affects(ch, output, obj->affected);
		}
}

/*
 * Parse a name for acceptability.
 */
bool pc_name_ok(const char *name)
{
	const unsigned char *pc;
	bool fIll,adjcaps = FALSE,cleancaps = FALSE;
 	int total_caps = 0;
	int i;

	/*
	 * Reserved words.
	 */
	if (is_name(name, "chronos all auto immortals self someone something"
			  "the you demise balance circle loner honor "
			  "none clan"))
		return FALSE;
	
	/*
	 * Length restrictions.
	 */
	 
	if (strlen(name) < 2)
		return FALSE;

	if (strlen(name) > MAX_CHAR_NAME)
		return FALSE;

	/*
	 * Alphanumerics only.
	 * Lock out IllIll twits.
	 */
	fIll = TRUE;
	for (pc = name; *pc != '\0'; pc++) {
		if (IS_SET(mud_options, OPT_ASCII_ONLY_NAMES) && !isascii(*pc))
			return FALSE;

		if (!isalpha(*pc))
			return FALSE;

		if (isupper(*pc)) { /* ugly anti-caps hack */
			if (adjcaps)
				cleancaps = TRUE;
			total_caps++;
			adjcaps = TRUE;
		}
		else
			adjcaps = FALSE;

		if (LOWER(*pc) != 'i' && LOWER(*pc) != 'l')
			fIll = FALSE;
	}

	if (fIll)
		return FALSE;

	if (total_caps > strlen(name) / 2)
		return FALSE;

	/*
	 * Prevent players from naming themselves after mobs.
	 */
	{
		MOB_INDEX_DATA *pMobIndex;
		int iHash;

		for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
			for (pMobIndex  = mob_index_hash[iHash];
			     pMobIndex != NULL; pMobIndex  = pMobIndex->next) 
				if (is_name(name, pMobIndex->name))
					return FALSE;
		}
	}

	for (i = 0; i < clans.nused; i++) {
		class_t *clan = VARR_GET(&clans, i);
		if (!str_cmp(name, clan->name))
			return FALSE;
	}

	return TRUE;
}

const char *stat_aliases[MAX_STATS][6] =
{
	{ "Titanic", "Herculian", "Strong", "Average", "Poor", "Weak"	},
	{ "Genious", "Clever", "Good", "Average", "Poor", "Hopeless"	},
	{ "Excellent", "Wise", "Good", "Average", "Dim", "Fool"		}, 	
	{ "Fast", "Quick", "Dextrous", "Average", "Clumsy", "Slow"	},
	{ "Iron", "Hearty", "Healthy", "Average", "Poor", "Fragile"	},
	{ "Charismatic", "Familier", "Good", "Average", "Poor", "Mongol"}
};

const char *get_stat_alias(CHAR_DATA *ch, int stat)
{
	int val;
	int i;

	if (stat >= MAX_STATS)
		return "Unknown";

	val = get_curr_stat(ch, stat);
	     if (val >  22)	i = 0;
	else if (val >= 20)	i = 1;
	else if (val >= 18)	i = 2;
	else if (val >= 14)	i = 3;
	else if (val >= 10)	i = 4;
	else			i = 5;
	return stat_aliases[stat][i];
}

/*****************************************************************************
 * some formatting stuff
 *
 */

/*
 * smash '~'
 */
const char *fix_short(const char *s)
{
	char *p;
	static char buf[MAX_STRING_LENGTH];

	if (!strchr(s, '~'))
		return s;

	for (p = buf; *s && p-buf < sizeof(buf)-1; s++) {
		if (*s == '~')
			continue;
		*p++ = *s;
	}

	*p = '\0';
	return buf;
}

const char *format_short(mlstring *mlshort, const char *name, CHAR_DATA *looker)
{
        static char buf[MAX_STRING_LENGTH];
        const char *sshort;

        sshort = fix_short(mlstr_cval(mlshort, looker));
	strnzcpy(buf, sizeof(buf), sshort);

        if (!IS_SET(looker->comm, COMM_NOENG)
	&&  sshort != mlstr_mval(mlshort)) {
		char buf2[MAX_STRING_LENGTH];
        	char buf3[MAX_STRING_LENGTH];

        	one_argument(name, buf3, sizeof(buf3));
		snprintf(buf2, sizeof(buf2), " (%s)", buf3);
		strnzcat(buf, sizeof(buf), buf2);
	}

        return buf;
}

/*
 * format description (long descr for mobs, description for objs)
 *
 * eng name expected to be in form " (foo)" and is stripped
 * if COMM_NOENG is set
 */
const char *format_descr(mlstring *ml, CHAR_DATA *looker)
{
	const char *s;
	const char *p, *q;
	static char buf[MAX_STRING_LENGTH];

	s = mlstr_cval(ml, looker);
	if (IS_NULLSTR(s)
	||  !IS_SET(looker->comm, COMM_NOENG)
	||  (p = strchr(s, '(')) == NULL
	||  (q = strchr(p+1, ')')) == NULL)
		return s;

	if (p != s && *(p-1) == ' ')
		p--;

	strnzncpy(buf, sizeof(buf), s, p-s);
	strnzcat(buf, sizeof(buf), q+1);
	return buf;
}

//#ifdef *WIN32
#if 1 // prool fool
void SET_ORG_RACE(CHAR_DATA *ch, int race)
{
	if (IS_NPC(ch))
		ch->pIndexData->race = race;
	else
		ch->pcdata->race = race;
}
#endif
