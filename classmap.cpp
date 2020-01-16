#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>


using namespace std;

#include "classmap.h"
#include "objects/object.h"
#include "character/character.h"
#include "collision_detection.h"
#include "file/file_io.h"
#include "game_mediator.h"

extern string FILEPATH;
extern graphicsLib graphLib;

#include "game.h"
extern game gameControl;

#include "inputlib.h"
extern inputLib input;

#include "soundlib.h"
extern soundLib soundManager;

#ifdef ANDROID
#include <android/log.h>
#endif

#include "file/file_io.h"
extern CURRENT_FILE_FORMAT::file_io fio;


extern CURRENT_FILE_FORMAT::file_game game_data;
extern CURRENT_FILE_FORMAT::file_stage stage_data;
extern CURRENT_FILE_FORMAT::st_save game_save;
extern CURRENT_FILE_FORMAT::file_map map_data[FS_STAGE_MAX_MAPS];

extern CURRENT_FILE_FORMAT::st_save game_save;

extern CURRENT_FILE_FORMAT::st_game_config game_config;

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
classMap::classMap() : stage_number(-1), number(-1), bg_scroll(st_float_position(0.0, 0.0)), fg_layer_scroll(st_float_position(0.0, 0.0)), _platform_leave_counter(0)
{

    for (int i=0; i<MAP_W; i++) {
		wall_scroll_lock[i] = false;
	}
    _water_bubble.pos.x = -1;
    _water_bubble.pos.y = -1;
    _water_bubble.x_adjust = 0;
    _water_bubble.timer = 0;
    _water_bubble.x_adjust_direction = ANIM_DIRECTION_LEFT;
    _3rd_level_ignore_area = st_rectangle(-1, -1, -1, -1);
    _level3_tiles = std::vector<struct st_level3_tile>();
    _show_map_pos_x = -1;

    graphLib.initSurface(st_size(RES_W+(TILESIZE*2), RES_H), &map_screen);
}


// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
classMap::~classMap()
{
    //std::cout << "map[" << number << "] destructor" << std::endl;
}


void classMap::reset_map()
{
    // reset objects
    for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
        object& temp_obj = (*it);
        // if object is a player item, remove it
        if (temp_obj.get_id() == game_data.player_items[0] || temp_obj.get_id() == game_data.player_items[1]) {
            temp_obj.set_finished(true);
        } else {
            temp_obj.reset();
        }
    }
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::setStageNumber(int setStageN) {
	//std::cout << "classMap::setStageNumber - setStageN: " << setStageN << std::endl;
	stage_number = setStageN;
}


short classMap::get_number() const
{
	return number;
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::setMapNumber(int setMapN) {
	number = setMapN;
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::loadMap()
{
	if (stage_number == -1) {
        graphLib.show_debug_msg("ERROR::loadStage invalid number[-1]");
		cout << "ERROR::map::loadMap - stage number was not set, can't load it before setting the number.\n";
		return;
	}
	if (number == -1) {
        graphLib.show_debug_msg("ERROR::loadStage invalid number[>MAX]");
		cout << "ERROR::map::loadMap - map number was not set, can't load it before setting the number.\n";
		return;
    }


    object_list.clear();

    _npc_list.clear();
    animation_list.clear();


	bool column_locked = true;
	for (int i=0; i<MAP_W; i++) {
		column_locked = true;
		for (int j=0; j<MAP_H; j++) {
            if (map_data[number].tiles[i][j].locked != TERRAIN_SOLID && map_data[number].tiles[i][j].locked != TERRAIN_DOOR && map_data[number].tiles[i][j].locked != TERRAIN_SCROLL_LOCK && map_data[number].tiles[i][j].locked != TERRAIN_ICE && map_data[number].tiles[i][j].locked != TERRAIN_SPIKE) {
				column_locked = false;
				break;
			}
		}
		wall_scroll_lock[i] = column_locked;
	}


    _level3_tiles.clear();

    for (int i=0; i<MAP_W; i++) {
        for (int j=0; j<MAP_H; j++) {
            int lvl3_x = map_data[number].tiles[i][j].tile3.x;
            int lvl3_y = map_data[number].tiles[i][j].tile3.y;
            if (lvl3_x != -1 && lvl3_y != -1) {
                //std::cout << "tile_lvl3[" << lvl3_x << "][" << lvl3_y << "]" << std::endl;
                struct st_level3_tile temp_tile(st_position(lvl3_x, lvl3_y), st_position(i, j));
                _level3_tiles.push_back(temp_tile);
            }
        }
    }

/*
    for (int i=0; i<MAP_W; i++) {
        for (int j=0; j<MAP_H; j++) {
            int lvl3_x = map_data[number].tiles[i][j].tile3.x;
            int lvl3_y = map_data[number].tiles[i][j].tile3.y;

            if (lvl3_x != -1 || lvl3_y != -1) {
                std::cout << "tile_lvl3[" << lvl3_x << "][" << lvl3_y << "]" << std::endl;
            }

            if (lvl3_x > -1 && lvl3_y != -1) {
                struct st_level3_tile temp_tile(st_position(lvl3_x, lvl3_y), st_position(i, j));
                _level3_tiles.push_back(temp_tile);
            } else if (lvl3_x < -1 && lvl3_y == 0) { // anim tiles
                lvl3_y = j*TILESIZE;
                struct st_level3_tile temp_tile(st_position(lvl3_x, lvl3_y), st_position(i, j));
                _level3_tiles.push_back(temp_tile);
            }
        }
    }
*/

	load_map_npcs();

    load_map_objects();

    create_dynamic_background_surfaces();

    init_animated_tiles();


#ifdef HANDLELD // portable consoles aren't strong enought for two dynamic backgrounds
map_data[number].backgrounds[0].speed = 0;
#endif

}


void classMap::showMap()
{
    draw_dynamic_backgrounds();
    if (get_map_gfx_mode() == SCREEN_GFX_MODE_BACKGROUND) {
        draw_lib.show_gfx();
    }

    // redraw screen, if needed
    if (_show_map_pos_x == -1 || abs(_show_map_pos_x - scroll.x) > TILESIZE) {
        draw_map_tiles();
    // use memory screen
    }
    int diff_scroll_x = scroll.x - _show_map_pos_x;
    graphLib.copyArea(st_rectangle(diff_scroll_x+TILESIZE, 0, RES_W, RES_H), st_position(0, 0), &map_screen, &graphLib.gameScreen);

    // draw animated tiles
    draw_animated_tiles(graphLib.gameScreen);

    if (get_map_gfx_mode() == SCREEN_GFX_MODE_FULLMAP) {
        draw_lib.show_gfx();
    }


}

void classMap::get_map_area_surface(graphicsLib_gSurface& mapSurface)
{
    graphLib.initSurface(st_size(RES_W, RES_H), &mapSurface);

    if (!mapSurface.get_surface()) {
        graphLib.show_debug_msg("EXIT #21.MALLOC");
        SDL_Quit();
        exit(-1);
    }

    graphLib.clear_surface_area(0, 0, RES_W, RES_H, map_data[number].background_color.r, map_data[number].background_color.g, map_data[number].background_color.b, mapSurface);

    draw_dynamic_backgrounds_into_surface(mapSurface);

    // redraw screen, if needed
    if (_show_map_pos_x == -1 || abs(_show_map_pos_x - scroll.x) > TILESIZE) {
        draw_map_tiles();
    // use memory screen
    }
    int diff_scroll_x = scroll.x - _show_map_pos_x;
    graphLib.copyArea(st_rectangle(diff_scroll_x+TILESIZE, 0, RES_W, RES_H), st_position(0, 0), &map_screen, &mapSurface);

    // draw animated tiles
    draw_animated_tiles(mapSurface);
}

void classMap::draw_map_tiles()
{

    _show_map_pos_x = scroll.x;

    int tile_x_ini = scroll.x/TILESIZE-1;
    if (tile_x_ini < 0) {
        tile_x_ini = 0;
    }


    graphLib.clear_surface(map_screen);

    // draw the tiles of the screen region
    struct st_position pos_origin;
    struct st_position pos_destiny;
    int n = -1;
    for (int i=tile_x_ini; i<tile_x_ini+(RES_W/TILESIZE)+3; i++) {
        int diff = scroll.x - (tile_x_ini+1)*TILESIZE;
        pos_destiny.x = n*TILESIZE - diff + TILESIZE;
        for (int j=0; j<MAP_H; j++) {

            // don't draw easy-mode blocks if game difficulty not set to easy

            if (map_data[number].tiles[i][j].locked == TERRAIN_EASYMODEBLOCK && game_save.difficulty == DIFFICULTY_EASY) {
                pos_destiny.y = j*TILESIZE;
                graphLib.place_easymode_block_tile(pos_destiny, map_screen);
            } else if (map_data[number].tiles[i][j].locked == TERRAIN_HARDMODEBLOCK && game_save.difficulty == DIFFICULTY_HARD) {
                pos_destiny.y = j*TILESIZE;
                graphLib.place_hardmode_block_tile(pos_destiny, map_screen);
            } else {
                pos_origin.x = map_data[number].tiles[i][j].tile1.x;
                pos_origin.y = map_data[number].tiles[i][j].tile1.y;

                if (pos_origin.x >= 0 && pos_origin.y >= 0) {
                    pos_destiny.y = j*TILESIZE;
                    graphLib.placeTile(pos_origin, pos_destiny, &map_screen);
                }
            }
        }
        n++;
    }
}

void classMap::draw_animated_tiles(graphicsLib_gSurface &surface)
{
    //scroll.x - dest.x
    for (int i=0; i<anim_tile_list.size(); i++) {
        //std::cout << "draw-anim-tile[" << i << "][" << anim_tile_list.at(i).anim_tile_id << "], x[" << anim_tile_list.at(i).dest_x << "], y[" << anim_tile_list.at(i).dest_y << "]" << std::endl;

        int pos_x = anim_tile_list.at(i).dest_x-scroll.x;
        if (pos_x >= -TILESIZE && pos_x <= RES_W+1) {
            //std::cout << "## scroll.x[" << scroll.x << "], dest.x[" << anim_tile_list.at(i).dest_x << "]" << std::endl;
            st_position dest_pos(pos_x, anim_tile_list.at(i).dest_y);
            graphLib.place_anim_tile(anim_tile_list.at(i).anim_tile_id, dest_pos, &surface);
        }
    }

    graphLib.update_anim_tiles_timers();
}

void classMap::init_animated_tiles()
{
    // draw the tiles of the screen region
    struct st_position pos_origin;
    struct st_position pos_destiny;
    for (int i=0; i<MAP_W; i++) {
        pos_destiny.x = i*TILESIZE;
        for (int j=0; j<MAP_H; j++) {
            pos_origin.x = map_data[number].tiles[i][j].tile1.x;
            pos_origin.y = map_data[number].tiles[i][j].tile1.y;

            if (pos_origin.x < -1 && pos_origin.y == 0) {
                int anim_tile_id = (pos_origin.x * -1) - 2;
                pos_destiny.y = j*TILESIZE;
                //std::cout << "MAP::showMap::place_anim_tile[" << i << "][" << j << "]" << std::endl;
                anim_tile_list.push_back(anim_tile_desc(anim_tile_id, pos_destiny));
            }
        }
    }
}


// ********************************************************************************************** //
// show the third level of tiles                                                                  //
// ********************************************************************************************** //
void classMap::showAbove(int scroll_y, int temp_scroll_x)
{
    int scroll_x = scroll.x;
    if (temp_scroll_x != -99999) {
        scroll_x = temp_scroll_x;
    }
	// only show pieces that in current screen position
    short start_point = scroll_x/TILESIZE;
	if (start_point > 0) { start_point--; }
    short end_point = (scroll_x+RES_W)/TILESIZE;
	if (end_point < MAP_W-1) { end_point++; }
    //std::cout << "showAbove - start_point: " << start_point << ", end_point: " << end_point << std::endl;


	// draw 3rd tile level
    std::vector<st_level3_tile>::iterator tile3_it;
    for (tile3_it = _level3_tiles.begin(); tile3_it != _level3_tiles.end(); tile3_it++) {

        if (_3rd_level_ignore_area.x != -1 && _3rd_level_ignore_area.w > 0 && ((*tile3_it).map_position.x >= _3rd_level_ignore_area.x && (*tile3_it).map_position.x < _3rd_level_ignore_area.x+_3rd_level_ignore_area.w && (*tile3_it).map_position.y >= _3rd_level_ignore_area.y && (*tile3_it).map_position.y < _3rd_level_ignore_area.y+_3rd_level_ignore_area.h)) {
            continue;
        }
        int pos_x = (*tile3_it).tileset_pos.x;
        int pos_y = (*tile3_it).tileset_pos.y;
        // only show tile if it is on the screen range

        graphLib.place_3rd_level_tile(pos_x, pos_y, ((*tile3_it).map_position.x*TILESIZE)-scroll_x, ((*tile3_it).map_position.y*TILESIZE)+scroll_y);
    }

    if (_water_bubble.pos.x != -1) {
        draw_lib.show_bubble(_water_bubble.pos.x+_water_bubble.x_adjust, _water_bubble.pos.y);
        int water_lock = getMapPointLock(st_position((_water_bubble.pos.x+2+scroll_x)/TILESIZE, _water_bubble.pos.y/TILESIZE));
        _water_bubble.pos.y -= 2;
        if (_water_bubble.x_adjust_direction == ANIM_DIRECTION_LEFT) {
            _water_bubble.x_adjust -= 0.5;
            if (_water_bubble.x_adjust < -4) {
                _water_bubble.x_adjust_direction = ANIM_DIRECTION_RIGHT;
            }
        } else {
            _water_bubble.x_adjust += 0.5;
            if (_water_bubble.x_adjust >= 0) {
                _water_bubble.x_adjust_direction = ANIM_DIRECTION_LEFT;
            }
        }
        if (water_lock != TERRAIN_WATER || _water_bubble.timer < timer.getTimer()) {
            //std::cout << ">> MAP::showAbove::HIDE_BUBBLE <<" <<std::endl;
            _water_bubble.pos.x = -1;
            _water_bubble.pos.y = -1;
        }
    }

    // animations
    /// @TODO: remove "finished" animations
    std::vector<animation>::iterator animation_it;
    for (animation_it = animation_list.begin(); animation_it != animation_list.end(); animation_it++) {
        if ((*animation_it).finished() == true) {
            animation_list.erase(animation_it);
            break;
        } else {
            (*animation_it).execute(); // TODO: must pass scroll map to npcs somwhow...
        }
    }

    draw_foreground_layer(scroll_x, scroll_y);


}

bool classMap::is_point_solid(st_position pos) const
{
	short int lock_p = getMapPointLock(pos);

    if (lock_p == TERRAIN_UNBLOCKED || lock_p != TERRAIN_WATER || lock_p == TERRAIN_CHECKPOINT || lock_p == TERRAIN_SCROLL_LOCK || (lock_p == TERRAIN_EASYMODEBLOCK && game_save.difficulty != 0)) {
        return false;
	}
    return true;
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
int classMap::getMapPointLock(st_position pos) const
{
    if (pos.x < 0 || pos.y < 0 || pos.y >= MAP_H || pos.x >= MAP_W) {
		return TERRAIN_UNBLOCKED;
	}
    return map_data[number].tiles[pos.x][pos.y].locked;
}

st_position_int8 classMap::get_map_point_tile1(st_position pos)
{
    if (pos.x < 0 || pos.y < 0 || pos.y > RES_H/TILESIZE || pos.x > MAP_W) {
        return st_position_int8(-1, -1);
    }
    return map_data[number].tiles[pos.x][pos.y].tile1;
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::changeScrolling(st_float_position pos, bool check_lock)
{
    /*
    if (timer.is_paused() == true) {
        return;
    }
    */

    //std::cout << "MAP::changeScrolling::timer: " << timer.getTimer() << ", pos.x: " << pos.x << std::endl;

    float bg1_speed = (float)map_data[number].backgrounds[0].speed/10;
    float foreground_layer_speed = (float)map_data[number].backgrounds[1].speed/10;

    //std::cout << "MAP::changeScrolling - foreground_layer_speed[" << foreground_layer_speed << "]" << std::endl;

	// moving player to right, screen to left
    if (pos.x > 0 && ((scroll.x/TILESIZE+RES_W/TILESIZE)-1 < MAP_W-1)) {
        int x_change = pos.x;
		if (pos.x >= TILESIZE) { // if change is too big, do not update (TODO: must check all wall until lock)
            x_change = 1;
		}
		int tile_x = (scroll.x+RES_W-TILESIZE+2)/TILESIZE;
		if (check_lock == false || wall_scroll_lock[tile_x] == false) {
            scroll.x += x_change;
            if (map_data[number].backgrounds[0].auto_scroll == BG_SCROLL_MODE_NONE) {
                bg_scroll.x -= ((float)x_change*bg1_speed);
            }
            fg_layer_scroll.x -= ((float)x_change*foreground_layer_speed);
            adjust_dynamic_background_position();
            adjust_foreground_position();
		}
	} else if (pos.x < 0) {
        int x_change = pos.x;
		if (pos.x < -TILESIZE) {
            x_change = -1;
		}
		if (scroll.x/TILESIZE >= 0) { // if change is too big, do not update (TODO: must check all wall until lock)
			int tile_x = (scroll.x+TILESIZE-2)/TILESIZE;
            //std::cout << "#2 LEFT changeScrolling - scroll.x: " << scroll.x << ", testing tile_x: " << tile_x << std::endl;
			if (check_lock == false || wall_scroll_lock[tile_x] == false) {
				//std::cout << "classMap::changeScrolling - 2" << std::endl;
                scroll.x += x_change;
                bg_scroll.x -= ((float)x_change*bg1_speed);
                fg_layer_scroll.x -= ((float)x_change*foreground_layer_speed);
                adjust_dynamic_background_position();
                adjust_foreground_position();
            }
		}
	}

	scroll.y += pos.y;
}


// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::set_scrolling(st_float_position pos)
{
	scrolled = pos;
	scroll.x = pos.x;
	scroll.y = pos.y;
    //std::cout << "------- classMap::set_scrolling - map: " << number << ", pos.x: " << pos.x << "-------" << std::endl;
}

void classMap::reset_scrolling()
{
    scrolled = st_position(0, 0);
    scroll.x = 0;
    scroll.y = 0;
}




// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
st_float_position classMap::getMapScrolling() const
{
    //std::cout << "getMapScrolling, x: " << scroll.x << ", y: " << scroll.y << std::endl;
    return scroll;
}

st_float_position *classMap::get_map_scrolling_ref()
{
    return &scroll;
}




// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::load_map_npcs()
{

    // remove all elements currently in the list
    if (_npc_list.size() > 0) {
        _npc_list.back().clean_character_graphics_list();
    }
    while (!_npc_list.empty()) {
        _npc_list.pop_back();
    }

	for (int i=0; i<MAX_MAP_NPC_N; i++) {
        if (map_data[number].map_npcs[i].id_npc != -1) {
            classnpc new_npc = classnpc(stage_number, number, map_data[number].map_npcs[i].id_npc, i);


            if (stage_data.boss.id_npc == map_data[number].map_npcs[i].id_npc) {
                new_npc.set_stage_boss(true);
            } else if (GameMediator::get_instance()->get_enemy(map_data[number].map_npcs[i].id_npc)->is_boss == true) {
                new_npc.set_is_boss(true);
            // adjust NPC position to ground, if needed
            } else if (new_npc.is_able_to_fly() == false && new_npc.hit_ground() == false) {
                new_npc.initialize_position_to_ground();
            }

            _npc_list.push_back(new_npc); // insert new npc at the list-end

        }


	}
}


void classMap::draw_dynamic_backgrounds()
{
    // only draw solid background color, if map-heigth is less than RES_H
    //std::cout << "number[" << number << "], bg1_surface.height[" << bg1_surface.height << "], bg1.y[" << map_data[number].backgrounds[0].adjust_y << "]" << std::endl;
    graphicsLib_gSurface* surface_bg = get_dynamic_bg();
    if (surface_bg == NULL || surface_bg->width <= 0) {
        graphLib.clear_surface_area(0, 0, RES_W, RES_H, map_data[number].background_color.r, map_data[number].background_color.g, map_data[number].background_color.b, graphLib.gameScreen);
        return;
    }
    if (surface_bg->width <= 0 || surface_bg->height < RES_H || map_data[number].backgrounds[0].adjust_y != 0) {
        graphLib.clear_surface_area(0, 0, RES_W, RES_H, map_data[number].background_color.r, map_data[number].background_color.g, map_data[number].background_color.b, graphLib.gameScreen);
    }

    float bg1_speed = (float)map_data[number].backgrounds[0].speed/10;
    int bg1_scroll_mode = map_data[number].backgrounds[0].auto_scroll;
    // dynamic background won't work in low-end graphics more
    if (game_config.graphics_performance_mode != PERFORMANCE_MODE_LOW) {
        if (bg1_scroll_mode == BG_SCROLL_MODE_LEFT) {
            bg_scroll.x -= ((float)1*bg1_speed);
            adjust_dynamic_background_position();
        } else if (bg1_scroll_mode == BG_SCROLL_MODE_RIGHT) {
            bg_scroll.x += ((float)1*bg1_speed);
            adjust_dynamic_background_position();
        } else if (bg1_scroll_mode == BG_SCROLL_MODE_UP) {
            bg_scroll.y -= ((float)1*bg1_speed);
            adjust_dynamic_background_position();
        } else if (bg1_scroll_mode == BG_SCROLL_MODE_DOWN) {
            bg_scroll.y += ((float)1*bg1_speed);
            adjust_dynamic_background_position();
        }
    }

    //std::cout << "## bg1_speed[" << bg1_speed << "], bg_scroll.x[" << bg_scroll.x << "]" << std::endl;

    int x1 = bg_scroll.x;
    if (x1 > 0) { // moving to right
        x1 = (RES_W - x1) * -1;
    }

    int y1 = bg_scroll.y + map_data[number].backgrounds[0].adjust_y;

    //std::cout << "## x1[" << x1 << "]" << std::endl;


    if (surface_bg->width > 0) {
        // draw leftmost part
        graphLib.copyAreaWithAdjust(st_position(x1, y1), surface_bg, &graphLib.gameScreen);

        // draw rightmost part, if needed
        if (abs(bg_scroll.x) > RES_W) {
            //std::cout << "### MUST DRAW SECOND BG-POS-LEFT ###" << std::endl;
            int bg_pos_x = RES_W - (abs(x1)-RES_W);
            graphLib.copyAreaWithAdjust(st_position(bg_pos_x, y1), surface_bg, &graphLib.gameScreen);
        }  else if (surface_bg->width - abs(bg_scroll.x) < RES_W) {
            int bg_pos_x = surface_bg->width - (int)abs(bg_scroll.x);
            graphLib.copyAreaWithAdjust(st_position(bg_pos_x, y1), surface_bg, &graphLib.gameScreen);
        }
    }
}

void classMap::draw_foreground_layer(int scroll_x, int scroll_y)
{

    if (strlen(map_data[number].backgrounds[1].filename) > 0) {
        float foreground_speed = (float)map_data[number].backgrounds[1].speed/10;
        int scroll_mode = map_data[number].backgrounds[1].auto_scroll;
        // dynamic background won't work in low-end graphics more
        if (game_config.graphics_performance_mode != PERFORMANCE_MODE_LOW) {
            if (scroll_mode == BG_SCROLL_MODE_LEFT) {
                fg_layer_scroll.x -= ((float)1*foreground_speed);
                adjust_foreground_position();
            } else if (scroll_mode == BG_SCROLL_MODE_RIGHT) {
                fg_layer_scroll.x += ((float)1*foreground_speed);
                adjust_foreground_position();
            } else if (scroll_mode == BG_SCROLL_MODE_UP) {
                fg_layer_scroll.y -= ((float)1*foreground_speed);
                adjust_foreground_position();
            } else if (scroll_mode == BG_SCROLL_MODE_DOWN) {
                fg_layer_scroll.y += ((float)1*foreground_speed);
                adjust_foreground_position();
            }
        }

        //std::cout << "## foreground_speed[" << foreground_speed << "], fg_layer_scroll.x[" << fg_layer_scroll.x << "]" << std::endl;

        int x1 = fg_layer_scroll.x;
        if (x1 > 0) { // moving to right
            x1 = (RES_W - x1) * -1;
        }

        int y1 = fg_layer_scroll.y + map_data[number].backgrounds[1].adjust_y;

        //std::cout << "## x1[" << x1 << "]" << std::endl;


        if (get_dynamic_foreground()->width > 0) {
            // draw leftmost part
            graphLib.copyAreaWithAdjust(st_position(x1, y1), get_dynamic_foreground(), &graphLib.gameScreen);

            // draw rightmost part, if needed
            //std::cout << "fg_layer_scroll.x[" << fg_layer_scroll.x << "]" << std::endl;
            if (abs(fg_layer_scroll.x) > RES_W) {
                //std::cout << "### MUST DRAW SECOND BG-POS-LEFT ###" << std::endl;
                int bg_pos_x = RES_W - (abs(x1)-RES_W);
                //std::cout << "Need to draw second part of surface, bg_pos_x[" << bg_pos_x << "]" << std::endl;
                graphLib.copyAreaWithAdjust(st_position(bg_pos_x, y1), get_dynamic_foreground(), &graphLib.gameScreen);
            }  else if (get_dynamic_foreground()->width - abs(fg_layer_scroll.x) < RES_W) {
                int foreground_pos_x = get_dynamic_foreground()->width - (int)abs(fg_layer_scroll.x);
                //std::cout << "### MUST DRAW SECOND BG-POS-RIGHT width[" << get_dynamic_foreground()->width << "], scroll.x[" << (int)abs(fg_layer_scroll.x) << "] ###" << std::endl;
                // test if there isn't a overlap, so we need to add +1
                graphLib.copyAreaWithAdjust(st_position(foreground_pos_x, y1), get_dynamic_foreground(), &graphLib.gameScreen);
            }

        }
    }


    // water tiles
    //std::cout << "draw_foreground_layer #2" << std::endl;
    int tile_x_ini = scroll.x/TILESIZE-1;
    if (tile_x_ini < 0) {
        tile_x_ini = 0;
    }
    struct st_position pos_destiny;
    int n = -1;

    for (int i=tile_x_ini; i<tile_x_ini+(RES_W/TILESIZE)+2; i++) {
        int diff = scroll.x - (tile_x_ini+1)*TILESIZE;
        pos_destiny.x = n*TILESIZE - diff;
        for (int j=0; j<MAP_H; j++) {
            pos_destiny.y = j*TILESIZE + scroll_y;
            // in high-end graphics mode, draw a blue transparent layer over water


            if (game_config.graphics_performance_mode == PERFORMANCE_MODE_HIGH && map_data[number].tiles[i][j].locked == TERRAIN_WATER) {
                //std::cout << "tile[" << i << "][" << j << "].locked[" << (int)map_data[number].tiles[i][j].locked << "], water[" << TERRAIN_WATER << "], perf-mode[" << (int)game_config.graphics_performance_mode << "]" << std::endl;
                graphLib.place_water_tile(pos_destiny);
            }
        }
        n++;
    }
}

void classMap::adjust_dynamic_background_position()
{
    if (get_dynamic_bg() == NULL) {
        return;
    }

    //int bg_limit = get_dynamic_bg()->width-RES_W;
    int bg_limit = get_dynamic_bg()->width;

    // esq -> direita: #1 bg_limt[640], scroll.x[-640.799]

    if (bg_scroll.x < -bg_limit) {
        //std::cout << "#1 bg_limt[" << bg_limit << "], scroll.x[" << bg_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #1" << std::endl;
        bg_scroll.x = 0;
    } else if (bg_scroll.x > bg_limit) {
        //std::cout << "#2 bg_limt[" << bg_limit << "], scroll.x[" << bg_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #2" << std::endl;
        bg_scroll.x = 0;
    } else if (bg_scroll.x > 0) {
        //std::cout << "#3 bg_limt[" << bg_limit << "], scroll.x[" << bg_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #3" << std::endl;
        bg_scroll.x = -(get_dynamic_bg()->width); // erro aqui
    }


    if (bg_scroll.y < -RES_H) {
        bg_scroll.y = 0;
    } else if (bg_scroll.y > RES_H) {
        bg_scroll.y = 0;
    }
}

void classMap::adjust_foreground_position()
{
    //int bg_limit = get_dynamic_foreground()->width-RES_W;
    int foreground_limit = get_dynamic_foreground()->width;

    // esq -> direita: #1 bg_limt[640], scroll.x[-640.799]

    if (fg_layer_scroll.x < -foreground_limit) {
        //std::cout << "#1 bg_limt[" << foreground_limit << "], scroll.x[" << fg_layer_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #1" << std::endl;
        fg_layer_scroll.x = 0;
    } else if (fg_layer_scroll.x > foreground_limit) {
        //std::cout << "#2 bg_limt[" << foreground_limit << "], scroll.x[" << fg_layer_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #2" << std::endl;
        fg_layer_scroll.x = 0;
    } else if (fg_layer_scroll.x > 0) {
        //std::cout << "#3 bg_limt[" << foreground_limit << "], scroll.x[" << fg_layer_scroll.x << "]" << std::endl;
        //std::cout << "RESET BG-SCROLL #3" << std::endl;
        fg_layer_scroll.x = -(get_dynamic_foreground()->width); // erro aqui
    }


    if (fg_layer_scroll.y < -RES_H) {
        fg_layer_scroll.y = 0;
    } else if (fg_layer_scroll.y > RES_H) {
        fg_layer_scroll.y = 0;
    }
}


void classMap::draw_dynamic_backgrounds_into_surface(graphicsLib_gSurface &surface)
{

    //std::cout << "MAP::draw_dynamic_backgrounds_into_surface - color: (" << map_data[number].background_color.r << ", " << map_data[number].background_color.g << ", " << map_data[number].background_color.b << ")" << std::endl;
    graphLib.clear_surface_area(0, 0, surface.width, surface.height, map_data[number].background_color.r, map_data[number].background_color.g, map_data[number].background_color.b, surface);


    if (get_dynamic_bg() == NULL) {
        return;
    }

    int x1 = bg_scroll.x;
    if (x1 > 0) { // moving to right
        x1 = (RES_W - x1) * -1;
    }

    int y1 = bg_scroll.y + map_data[number].backgrounds[0].adjust_y;


    if (get_dynamic_bg()->width > 0) {
        // draw leftmost part
        graphLib.copyAreaWithAdjust(st_position(x1, y1), get_dynamic_bg(), &surface);

        // draw rightmost part, if needed
        //std::cout << "bg_scroll.x[" << bg_scroll.x << "]" << std::endl;
        if (abs(bg_scroll.x) > RES_W) {
            //std::cout << "### MUST DRAW SECOND BG-POS-LEFT ###" << std::endl;
            int bg_pos_x = RES_W - (abs(x1)-RES_W);
            graphLib.copyAreaWithAdjust(st_position(bg_pos_x, y1), get_dynamic_bg(), &surface);
        }  else if (get_dynamic_bg()->width - abs(bg_scroll.x) < RES_W) {
            int bg_pos_x = get_dynamic_bg()->width - abs(bg_scroll.x);
            //std::cout << "### MUST DRAW SECOND BG-POS-RIGHT bg_pos_x[" << bg_pos_x << "] ###" << std::endl;
            graphLib.copyAreaWithAdjust(st_position(bg_pos_x, y1), get_dynamic_bg(), &surface);
        }

    }

}

void classMap::add_object(object obj)
{
    object_list.push_back(obj);
}

int classMap::get_first_lock_on_left(int x_pos) const
{
    for (int i=x_pos; i>= 0; i--) {
        if (wall_scroll_lock[i] == true) {
            return i*TILESIZE;
        }
    }
    return -1;
}

int classMap::get_first_lock_on_right(int x_pos) const
{
    int limit = (scroll.x+RES_W)/TILESIZE;
    x_pos += 1;
    //std::cout << "classMap::get_first_lock_on_right - x_pos: " << x_pos << ", limit: " << limit << std::endl;
    for (int i=x_pos; i<=limit; i++) {
        if (wall_scroll_lock[i] == true) {
            //std::cout << "classMap::get_first_lock_on_right - found lock at: " << i << std::endl;
            return i*TILESIZE;
        }
    }
    return -1;
}

// gets the first tile locked that have at least 3 tiles unlocked above it
int classMap::get_first_lock_on_bottom(int x_pos)
{
    int tilex = x_pos/TILESIZE;

    for (int i=MAP_H-1; i>=4; i--) { // ignore here 3 first tiles, as we need to test them next

        //std::cout << "STAGE::get_teleport_minimal_y[" << i << "]" << std::endl;

        int map_lock = getMapPointLock(st_position(tilex, i));
        bool found_bad_point = false;
        if (map_lock != TERRAIN_UNBLOCKED && map_lock != TERRAIN_WATER) {
            // found a stop point, now check above tiles
            for (int j=i-1; j>=i-3; j--) {
                int map_lock2 = getMapPointLock(st_position(tilex, j));
                if (map_lock2 != TERRAIN_UNBLOCKED && map_lock2 != TERRAIN_WATER) { // found a stop point, now check above ones
                    found_bad_point = true;
                    break;
                }
            }
            if (found_bad_point == false) {
                return i-1;
            }
        }
    }
    return 0;
}

void classMap::drop_item(int i)
{
    st_position position = st_position(_npc_list.at(i).getPosition().x + _npc_list.at(i).get_size().width/2, _npc_list.at(i).getPosition().y + _npc_list.at(i).get_size().height/2);
    // dying out of screen should not drop item
    if (position.y > RES_H) {
        return;
    }
    int rand_n = rand() % 100;
    //std::cout << ">>>>>>> classMap::drop_item - rand_n: " << rand_n << std::endl;
    DROP_ITEMS_LIST obj_type;
    if (rand_n <= 10) {
        //std::cout << ">>>>>>> classMap::drop_item - DROP_ITEM_ENERGY_SMALL" << std::endl;
        obj_type = DROP_ITEM_ENERGY_SMALL;
    } else if (rand_n <= 20) {
        //std::cout << ">>>>>>> classMap::drop_item - DROP_ITEM_WEAPON_SMALL" << std::endl;
        obj_type = DROP_ITEM_WEAPON_SMALL;
    } else if (rand_n <= 25) {
        //std::cout << ">>>>>>> classMap::drop_item - DROP_ITEM_ENERGY_BIG" << std::endl;
        obj_type = DROP_ITEM_ENERGY_BIG;
    } else if (rand_n <= 30) {
        //std::cout << ">>>>>>> classMap::drop_item - DROP_ITEM_WEAPON_BIG" << std::endl;
        obj_type = DROP_ITEM_WEAPON_BIG;
    } else {
        return;
    }

    st_position obj_pos;
    obj_pos.y = position.y/TILESIZE;
    obj_pos.x = (position.x - TILESIZE)/TILESIZE;

    // sub-bosses always will drop energy big
    if (_npc_list.at(i).is_subboss()) {
        obj_type = DROP_ITEM_ENERGY_BIG;
    }

    short obj_type_n = gameControl.get_drop_item_id(obj_type);
    if (obj_type_n == -1) {
        //std::cout << ">>>>>>>>> obj_type_n(" << obj_type_n << ") invalid for obj_type(" << obj_type << ")" << std::endl;
        return;
    }
    object temp_obj(obj_type_n, this, obj_pos, st_position(-1, -1), -1);
    temp_obj.set_position(position);
    temp_obj.set_duration(4500);
    add_object(temp_obj);
}

void classMap::set_bg_scroll(int scrollx)
{
    bg_scroll.x = scrollx;
}

int classMap::get_bg_scroll() const
{
    return bg_scroll.x;
}


void classMap::reset_objects_timers()
{
    //std::cout << ">>>>>> MAP::reset_objects_timers - object_list.size: " << object_list.size() << std::endl;
    std::vector<object>::iterator object_it;
    for (object_it = object_list.begin(); object_it != object_list.end(); object_it++) {
        (*object_it).reset_timers(); // TODO: must pass scroll map to npcs somwhow...
    }
}

void classMap::reset_objects()
{
    //std::cout << ">>>>>> MAP::reset_objects - object_list.size: " << object_list.size() << std::endl;
    std::vector<object>::iterator object_it;
    for (object_it = object_list.begin(); object_it != object_list.end(); object_it++) {
        (*object_it).reset();
    }
}

void classMap::print_objects_number()
{
    //std::cout << ">>>>>> MAP::print_objects_number - n: " << object_list.size() << std::endl;
}

void classMap::add_bubble_animation(st_position pos)
{
    if (_water_bubble.timer > timer.getTimer()) {
        //std::cout << ">> MAP::add_bubble::CANT_ADD <<" <<std::endl;
        return;
    }
    //std::cout << ">> MAP::add_bubble::ADDED <<" <<std::endl;
    _water_bubble.timer = timer.getTimer()+3000;
    _water_bubble.pos.x = pos.x;
    _water_bubble.pos.y = pos.y;
    _water_bubble.x_adjust = 0;
}

// checks if player have any special object in screen
bool classMap::have_player_object()
{
    for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
        object& temp_obj = (*it);
        int item_id = temp_obj.get_id();
        if (item_id == game_data.player_items[0] || item_id == game_data.player_items[1]) {
            return true;
        }
    }
    return false;
}

bool classMap::subboss_alive_on_left(short tileX)
{
    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_subboss() == true && _npc_list.at(i).is_dead() == false) {
            //std::cout << "Opa, achou um sub-boss!" << std::endl;
            int dist_door_npc = tileX*TILESIZE - _npc_list.at(i).getPosition().x;
            //std::cout << "dist_door_npc[" << dist_door_npc << "], NPC-pos.x: " << _npc_list.at(i).getPosition().x << ", tileX*TILESIZE: " << tileX*TILESIZE << std::endl;
            if (_npc_list.at(i).getPosition().x >= (tileX-20)*TILESIZE && _npc_list.at(i).getPosition().x <= tileX*TILESIZE) { // 20 tiles is the size of a visible screen
                //std::cout << "Opa, achou um sub-boss NA ESQUERDA!!" << std::endl;
                return true;
            }
        }
    }
    return false;
}

void classMap::finish_object_teleporter(int number)
{
    for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
        object& temp_obj = (*it);
        //std::cout << "number: " << number << ", obj.id: " << temp_obj.get_obj_map_id() << std::endl;
        if (temp_obj.get_obj_map_id() == number) {
            temp_obj.set_direction(ANIM_DIRECTION_RIGHT);
        }
    }
}

void classMap::activate_final_boss_teleporter()
{
    for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
        object& temp_obj = (*it);
        //std::cout << "number: " << number << ", obj.id: " << temp_obj.get_obj_map_id() << ", type: " << temp_obj.get_type() << ", OBJ_FINAL_BOSS_TELEPORTER: " << OBJ_FINAL_BOSS_TELEPORTER << std::endl;
        if (temp_obj.get_type() == OBJ_FINAL_BOSS_TELEPORTER) {
            temp_obj.start();
        }
    }
}

Uint8 classMap::get_map_gfx()
{
    //std::cout << ">> MAP::get_map_gfx[" << number << "]" << std::endl;
    return map_data[number].backgrounds[0].gfx;
}

Uint8 classMap::get_map_gfx_mode()
{
    return map_data[number].backgrounds[1].auto_scroll;
}

st_float_position classMap::get_bg_scroll()
{
    return bg_scroll;
}

void classMap::set_bg_scroll(st_float_position pos)
{
    bg_scroll = pos;
}

st_rectangle classMap::get_player_hitbox()
{
    return _player_ref->get_hitbox();
}





// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::load_map_objects() {
	std::map<std::string, object>::iterator it;

	// remove all elements currently in the list
    while (object_list.size() > 0) {
        object_list.pop_back();
    }

    while (animation_list.size() > 0) {
        animation_list.pop_back();
    }

	for (int i=0; i<MAX_MAP_NPC_N; i++) {
        if (map_data[number].map_objects[i].id_object != -1) {
            //int temp_id = map_data[number].map_objects[i].id_object;
            object temp_obj(map_data[number].map_objects[i].id_object, this, map_data[number].map_objects[i].start_point, map_data[number].map_objects[i].link_dest, map_data[number].map_objects[i].map_dest);
            temp_obj.set_obj_map_id(i);
            temp_obj.set_direction(map_data[number].map_objects[i].direction);
			object_list.push_back(temp_obj);
		}
	}
}





st_float_position classMap::get_last_scrolled() const
{
	return scrolled;
}

// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
void classMap::reset_scrolled()
{
	scrolled.x = 0;
	scrolled.y = 0;
}

bool classMap::value_in_range(int value, int min, int max) const
{
    return (value >= min) && (value <= max);
}

void classMap::create_dynamic_background_surfaces()
{
    std::string bg_filename = std::string(map_data[number].backgrounds[0].filename);
    if (bg_filename.length() > 0) {
        //std::cout << "MAP[" << (int)number << "]::create_bg[" << bg_filename << "]" << std::endl;
        draw_lib.add_dynamic_background(bg_filename, map_data[number].backgrounds[0].auto_scroll, map_data[number].background_color);
    }
    // foreground image
    if (strlen(map_data[number].backgrounds[1].filename) > 0) {
        draw_lib.add_dynamic_background(std::string(map_data[number].backgrounds[1].filename), map_data[number].backgrounds[1].auto_scroll, st_color(COLORKEY_R, COLORKEY_G, COLORKEY_B));
        if (map_data[number].backgrounds[1].gfx != 100) {
            int fg_alpha = (255 * map_data[number].backgrounds[1].gfx)/100;
            //std::cout << ">>>>>>>>>>>>>>> FG-Alpha[" << fg_alpha << "]" << std::endl;
            graphLib.set_surface_alpha(fg_alpha, get_dynamic_foreground());
        }
    }
}

graphicsLib_gSurface *classMap::get_dynamic_bg()
{
    return draw_lib.get_dynamic_background(map_data[number].backgrounds[0].filename);
}

graphicsLib_gSurface *classMap::get_dynamic_foreground()
{
    return draw_lib.get_dynamic_foreground(map_data[number].backgrounds[1].filename);
}



int classMap::collision_rect_player_obj(st_rectangle player_rect, object* temp_obj, const short int x_inc, const short int y_inc, const short obj_xinc, const short obj_yinc)
{
    int blocked = 0;
    int obj_y_reducer = 1;
    collision_detection rect_collision_obj;

// used to give char a small amount of pixels that he can enter inside object image

    st_position temp_obj_pos = temp_obj->get_position();

    st_rectangle obj_rect(temp_obj_pos.x+obj_xinc, temp_obj_pos.y+obj_yinc+obj_y_reducer, temp_obj->get_size().width, temp_obj->get_size().height);
    st_rectangle p_rect(player_rect.x+x_inc, player_rect.y+y_inc, player_rect.w, player_rect.h);

    // if only moving up/down, give one extra pivel free (otherwise in won't be able to jump next an object)

    if (x_inc == 0 && y_inc != 0) {
        p_rect.x++;
        p_rect.w -= 2;
    }

    if (temp_obj->get_type() == OBJ_ITEM_JUMP) {
        obj_rect.y += OBJ_JUMP_Y_ADJUST;
    } else {
        obj_rect.y += 1;
    }

    bool xObjOver = value_in_range(obj_rect.x, p_rect.x, p_rect.x + p_rect.w);
    bool xPlayerOver = value_in_range(p_rect.x, obj_rect.x, obj_rect.x + obj_rect.w);
    bool xOverlap = xObjOver == true || xPlayerOver == true;
    bool yOverlap = value_in_range(obj_rect.y, p_rect.y, p_rect.y + p_rect.h) || value_in_range(p_rect.y, obj_rect.y, obj_rect.y + obj_rect.h);

    // check if X is blocked
    bool before_collision = rect_collision_obj.rect_overlap(obj_rect, p_rect);
    if (before_collision == true && temp_obj->get_collision_mode() != COLlISION_MODE_Y) {
        blocked = BLOCK_X;
    }



    if (xOverlap == true && yOverlap == true) {
        if (blocked == 0) {
            blocked = BLOCK_Y;
        } else {
            blocked = BLOCK_XY;
        }
    }

    if (blocked != 0 && temp_obj->get_type() == OBJ_ACTIVE_OPENING_SLIM_PLATFORM) {
        if (abs(p_rect.y + p_rect.h - obj_rect.y) > y_inc || y_inc < 0) {
            //std::cout << "SLIM - ignore block, y_inc[" << y_inc << "]" << std::endl;
            blocked = 0;
        }
    }

    //std::cout << "blocked: " << blocked << ", xOverlap: " << xOverlap << ", yOverlap: " << yOverlap << ", p.x: " << p_rect.x << ", p.y: " << p_rect.y << ", p.w: " << p_rect.w << ", p.h: " << p_rect.h << ", o.y: " << obj_rect.y << ", y_inc: " << y_inc << std::endl;


    return blocked;
}


// this method is used for npcs to ignore certain objects
bool classMap::is_obj_ignored_by_enemies(Uint8 obj_type)
{
    if (obj_type == OBJ_ENERGY_TANK) {
        return true;
    }
    if (obj_type == OBJ_WEAPON_TANK) {
        return true;
    }
    if (obj_type == OBJ_ENERGY_PILL_BIG) {
        return true;
    }
    if (obj_type == OBJ_WEAPON_PILL_BIG) {
        return true;
    }
    if (obj_type == OBJ_ENERGY_PILL_SMALL) {
        return true;
    }
    if (obj_type == OBJ_WEAPON_PILL_SMALL) {
        return true;
    }
    if (obj_type == OBJ_LIFE) {
        return true;
    }
    if (obj_type == OBJ_ITEM_FLY) {
        return true;
    }
    if (obj_type == OBJ_ITEM_JUMP) {
        //std::cout << "IGNORE OBJ_ITEM_JUMP" << std::endl;
        return true;
    }
    if (obj_type == OBJ_ARMOR_ARMS) {
        return true;
    }
    if (obj_type == OBJ_ARMOR_BODY) {
        return true;
    }
    if (obj_type == OBJ_ARMOR_LEGS) {
        return true;
    }
    if (obj_type == OBJ_BOSS_TELEPORTER) {
        return true;
    }
    if (obj_type == OBJ_SPECIAL_TANK) {
        return true;
    }
    if (obj_type == OBJ_FINAL_BOSS_TELEPORTER) {
        return true;
    }
    return false;
}


void classMap::collision_char_object(character* charObj, const float x_inc, const short int y_inc)
{
    int blocked = 0;
    object* res_obj = NULL;

    //if (y_inc < 0) std::cout << "MAP::collision_player_object - y_inc: " << y_inc << std::endl;

    // ignore collision if teleporting
    if (charObj->get_anim_type() == ANIM_TYPE_TELEPORT) {
        return;
    }

    st_rectangle char_rect = charObj->get_hitbox();

    //std::cout << ">>>>>>>>> CLASSMAP::collision-player-object, char_rect.y[" << char_rect.y << "]" << std::endl;

    /// @TODO: isso aqui deveria mesmo estar aqui?
    if (charObj->get_platform() == NULL) {
        for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
            object& temp_obj = (*it);

            if (temp_obj.is_hidden() == true) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #1" << std::endl;
                continue;
            }

            if (temp_obj.is_on_screen() == false) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #2" << std::endl;
                continue;
            }

            if (temp_obj.finished() == true) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #3" << std::endl;
                continue;
            }

            if (charObj->is_player() == false && is_obj_ignored_by_enemies(temp_obj.get_type())) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #4" << std::endl;
                continue;
            }

            // slim platform won't collide if movement is from bottom to top
            if (temp_obj.get_type() == OBJ_ACTIVE_OPENING_SLIM_PLATFORM && y_inc < 0) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #5" << std::endl;
                continue;
            }

            if (temp_obj.is_teleporting()) {
                //std::cout << "obj[" << temp_obj.get_name() << "] - leave #6 [teleporting object]" << std::endl;
                continue;
            }

            // jumping from inside item-coil must not block player
            if (temp_obj.get_type() == OBJ_ITEM_JUMP && y_inc < 0) {
                continue;
            }

            //std::cout << "### obj[" << temp_obj.get_name() << "] - CHECK #1 ###" << std::endl;


            // some platforms can kill the player if he gets stuck inside it
            if (charObj->is_player() == true && (temp_obj.get_type() == OBJ_MOVING_PLATFORM_UPDOWN || temp_obj.get_type() == OBJ_FLY_PLATFORM)) {
                st_rectangle stopped_char_rect = charObj->get_hitbox();
                stopped_char_rect.x+= CHAR_OBJ_COLlISION_KILL_ADJUST/2;
                stopped_char_rect.y+= CHAR_OBJ_COLlISION_KILL_ADJUST;
                stopped_char_rect.w-= CHAR_OBJ_COLlISION_KILL_ADJUST;
                stopped_char_rect.h-= CHAR_OBJ_COLlISION_KILL_ADJUST*2;

                //std::cout << "collision_rect_player_obj::CALL #1" << std::endl;
                // check if, without moving, player is inside object
                int no_move_blocked = collision_rect_player_obj(stopped_char_rect, &temp_obj, 0, 0, 0, 0);
                if (no_move_blocked == BLOCK_XY) {
                    _obj_collision = object_collision(BLOCK_INSIDE_OBJ, &temp_obj);
                    //std::cout << "obj[" << temp_obj.get_name() << "] - leave #5" << std::endl;
                    return;
                }
            }

            // usar TEMP_BLOCKED aqui, para não zerar o blocked anterior, fazer merge dos valores
            int temp_blocked = 0;
            //std::cout << "collision_rect_player_obj::CALL #2" << std::endl;
            temp_blocked = collision_rect_player_obj(char_rect, &temp_obj, x_inc, y_inc, 0, 0);
            //std::cout << "### obj[" << temp_obj.get_name() << "] - CHECK::temp_blocked[" << temp_blocked << "] ###" << std::endl;


            int temp_obj_y = temp_obj.get_position().y;
            if (temp_obj.get_type() == OBJ_ITEM_JUMP) {
                temp_obj_y += OBJ_JUMP_Y_ADJUST;
            }

            // to enter platform, player.x+player.h must not be much higher than obj.y
            if (temp_blocked != 0 && temp_obj.is_platform()) {



                //std::cout << "### obj[" << temp_obj.get_name() << "] - CHECK #2, temp_blocked[" << temp_blocked << "] ###" << std::endl;


                if (char_rect.y+char_rect.h-2 > temp_obj_y) {

                    //std::cout << "temp_blocked[" << temp_obj.get_name() << "] RESET BLOCK" << std::endl;
                    // this avoids that player gets stuck inside an object
                    /// @TODO: only do that if player is trying to leave object area (is locked even with xinc zero)
                    //temp_blocked = 0;
                //} else {
                    //std::cout << "temp_blocked[" << temp_obj.get_name() << "] KEEP! BLOCK" << std::endl;
                }
            }


            if (temp_blocked == BLOCK_Y || temp_blocked == BLOCK_XY) {

                bool entered_platform = false;

                if (temp_obj.get_state() != 0 && temp_obj.get_type() == OBJ_TRACK_PLATFORM) {
                    continue;
                }

                if (temp_obj.get_state() != 0 && (temp_obj.get_type() == OBJ_RAY_VERTICAL || temp_obj.get_type() == OBJ_RAY_HORIZONTAL)) {
                    //std::cout << "############# RAY.DAMAGE #############" << std::endl;
                    charObj->damage(TOUCH_DAMAGE_BIG, false);
                    continue;
                } else if (temp_obj.get_state() != 0 && (temp_obj.get_type() == OBJ_DEATHRAY_VERTICAL || temp_obj.get_type() == OBJ_DEATHRAY_HORIZONTAL)) {
                    //std::cout << "DEATHRAY(damage) - player.x: " << char_rect.x << ", map.scroll_x: " << scroll.x << ", pos.x: " << temp_obj.get_position().x << ", size.w: " << temp_obj.get_size().width << std::endl;
                    charObj->damage(999, false);
                    continue;
                }

                //std::cout << "y_inc[" << y_inc << "], char_rect.y[" << char_rect.y << "], temp_obj_y[" << temp_obj_y << "]" << std::endl;

                if (y_inc > 0 && char_rect.y <= temp_obj_y) {
                    //std::cout << ">>>>>>>> entered_platform!!!!!!! <<<<<" << std::endl;
                    entered_platform = true;
                }


                if (entered_platform == true) {
                    //std::cout << "player.platform: " << playerObj->get_platform() << std::endl;


                    if (temp_obj.is_hidden() == false && (temp_obj.get_type() == OBJ_MOVING_PLATFORM_UPDOWN || temp_obj.get_type() == OBJ_MOVING_PLATFORM_LEFTRIGHT || temp_obj.get_type() == OBJ_DISAPPEARING_BLOCK)) {
                        if (charObj->get_platform() == NULL && (temp_blocked == 2 || temp_blocked == 3)) {
                            charObj->set_platform(&temp_obj);
                            if (temp_obj.get_type() == OBJ_FALL_PLATFORM) {
                                temp_obj.set_direction(ANIM_DIRECTION_LEFT);
                            }
                        } else if (charObj->get_platform() == NULL && temp_blocked == 1) {
                            charObj->set_platform(&temp_obj);
                        }
                        if (temp_blocked != 0) {
                            _obj_collision = object_collision(temp_blocked, &(*it));
                            return;
                        }
                    } else if (temp_obj.get_type() == OBJ_ITEM_FLY) {
                        if (charObj->get_platform() == NULL && (temp_blocked == 2 || temp_blocked == 3) && y_inc > 0) {
                            charObj->set_platform(&temp_obj);
                            if (temp_obj.get_distance() == 0) {
                                temp_obj.start();
                                temp_obj.set_distance(1);
                                temp_obj.set_timer(timer.getTimer()+30);
                            }
                        }
                        if (temp_blocked != 0) {
                            _obj_collision = object_collision(temp_blocked, &(*it));
                            return;
                        }
                    } else if (temp_obj.get_type() == OBJ_ITEM_JUMP) {
                        if (charObj->get_platform() == NULL && (temp_blocked == 2 || temp_blocked == 3) && y_inc > 0 && charObj->getPosition().y+charObj->get_size().height <= temp_obj_y+1) {
                            charObj->activate_super_jump();
                            charObj->activate_force_jump();
                            temp_obj.start();
                        }
                        if (temp_blocked != 0) {
                            if (y_inc > 0) {
                                //std::cout << ">>>> temp_blocked: " << temp_blocked << ", y_inc: " << y_inc << std::endl;
                                _obj_collision = object_collision(temp_blocked, &(*it));
                                return;
                            } else {
                                //std::cout << ">>>> RESET BLOCKED" <<  std::endl;
                                temp_blocked = 0;
                            }
                        }
                    } else if (temp_obj.is_hidden() == false && temp_obj.is_started() == false && (temp_obj.get_type() == OBJ_ACTIVE_DISAPPEARING_BLOCK || temp_obj.get_type() == OBJ_ACTIVE_OPENING_SLIM_PLATFORM)) {
                        temp_obj.start();
                    } else if (temp_obj.get_type() == OBJ_FALL_PLATFORM || temp_obj.get_type() == OBJ_FLY_PLATFORM) {
                        if (charObj->get_platform() == NULL) {
                            charObj->set_platform(&temp_obj);
                            if (temp_obj.get_state() == OBJ_STATE_STAND) {
                                temp_obj.set_state(OBJ_STATE_MOVE);
                                temp_obj.start();
                            }
                            temp_obj.set_timer(timer.getTimer()+30);
                            _obj_collision = object_collision(temp_blocked, &(*it));
                            return;
                        }
                    } else if (temp_obj.get_type() == OBJ_TRACK_PLATFORM) {
                        if (charObj->get_platform() == NULL) {
                            charObj->set_platform(&temp_obj);
                            _obj_collision = object_collision(temp_blocked, &(*it));
                            return;
                        }
                    } else if (temp_obj.get_type() == OBJ_DAMAGING_PLATFORM) {
                        if (charObj->get_platform() == NULL) {
                            charObj->set_platform(&temp_obj);
                            _obj_collision = object_collision(temp_blocked, &(*it));
                            temp_obj.start();
                            return;
                        }
                    }


                }

                if (temp_blocked != 0) {
                    res_obj = &(*it);
                }

            }



            // merge blocked + temp_blocked
            if (temp_blocked == BLOCK_X) {
                if (blocked == 0) {
                    blocked = BLOCK_X;
                } else if (blocked == BLOCK_Y) {
                    blocked = BLOCK_XY;
                }
            } else if (temp_blocked == BLOCK_Y) {
                if (blocked == 0) {
                    blocked = BLOCK_Y;
                } else if (blocked == BLOCK_X) {
                    blocked = BLOCK_XY;
                }
            } else if (temp_blocked == BLOCK_XY) {
                blocked = BLOCK_XY;
            }
        }




    // this part seems to be OK
    } else {
        object* temp_obj = charObj->get_platform();
        if (temp_obj->is_hidden() == true) {
            //std::cout << "obj[" << temp_obj->get_name() << "] - leave #2.1" << std::endl;
            charObj->set_platform(NULL);
        } else if (temp_obj->get_type() == OBJ_TRACK_PLATFORM && temp_obj->get_state() != 0) {
            //std::cout << "obj[" << temp_obj->get_name() << "] - leave #2.2" << std::endl;
            charObj->set_platform(NULL);
        } else {
            //std::cout << "collision_rect_player_obj::CALL #3" << std::endl;
            blocked = collision_rect_player_obj(char_rect, temp_obj, x_inc, y_inc, 0, 0);
            if (blocked != 0) {
                res_obj = temp_obj;
            }
            //std::cout << "IN-PLATFORM[" << temp_obj->get_name() << "], blocked[" << blocked << "], y_inc[" << y_inc << "]" << std::endl;
        }
    }


    // got out of platform
    if (blocked == 0 && charObj->get_platform() != NULL) {
        //  for player item, platform must only be removed only if the item was already adtivated
        if (charObj->get_platform()->get_type() == OBJ_ITEM_FLY || charObj->get_platform()->get_type() == OBJ_ITEM_JUMP) {
            //std::cout << "### DEBUG OBJ_ITEM_JUMP #1 ###" << std::endl;
            if (charObj->get_platform()->get_distance() > 0 && y_inc != 0) {
                //std::cout << "CHAR::OUT-PLATFORM #1" << std::endl;
                charObj->set_platform(NULL);
            } else {
                _obj_collision = object_collision(0, NULL);
                return;
            }
        } else if (charObj->get_platform()->is_hidden() == true) {
            //std::cout << ">> OUT OF PLATFORM #2" << std::endl;
            charObj->set_platform(NULL);
        } else {
            _platform_leave_counter++;
            if (_platform_leave_counter > 2) {
                //std::cout << ">> OUT OF PLATFORM #3" << std::endl;
                charObj->set_platform(NULL);
                _platform_leave_counter = 0;
            }
        }
    } else if (blocked != 0 && charObj->get_platform() != NULL) {
        _platform_leave_counter = 0;
    }


    _obj_collision = object_collision(blocked, res_obj);
}

object_collision classMap::get_obj_collision()
{
    return _obj_collision;
}




void classMap::clean_map_npcs_projectiles()
{
    for (int i=0; i<_npc_list.size(); i++) {
        _npc_list.at(i).clean_projectiles();
    }
}

void classMap::reset_beam_objects()
{
    // reset objects
    for (std::vector<object>::iterator it=object_list.begin(); it!=object_list.end(); it++) {
        object& temp_obj = (*it);
        short obj_type = temp_obj.get_type();
        if (obj_type == OBJ_DEATHRAY_VERTICAL || obj_type == OBJ_DEATHRAY_HORIZONTAL || obj_type == OBJ_RAY_VERTICAL || obj_type == OBJ_RAY_HORIZONTAL) {
            temp_obj.reset();
        }
    }
}



bool classMap::get_map_point_wall_lock(int x) const
{
	return wall_scroll_lock[x/TILESIZE];
}

void classMap::move_map(const short int move_x, const short int move_y)
{
    set_scrolling(st_float_position(scroll.x+move_x, scroll.y+move_y));
}


// ********************************************************************************************** //
//                                                                                                //
// ********************************************************************************************** //
classnpc* classMap::collision_player_npcs(character* playerObj, const short int x_inc, const short int y_inc)
{
    UNUSED(x_inc);
    UNUSED(y_inc);
	struct st_rectangle p_rect, npc_rect;

    p_rect = playerObj->get_hitbox();

    //std::cout << "collision_player_npcs - p1.x: " << p1.x << ", p1.y: " << p1.y << std::endl;

    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_player_friend() == true) {
            //std::cout << "collision_player_npcs - FRIEND" << std::endl;
			continue;
		}
        if (_npc_list.at(i).is_dead() == true) {
            //std::cout << "collision_player_npcs - DEAD" << std::endl;
			continue;
		}
        if (_npc_list.at(i).is_invisible() == true) {
            //std::cout << "collision_player_npcs - INVISIBLE" << std::endl;
			continue;
		}

        if (_npc_list.at(i).is_on_visible_screen() == false) {
            continue;
        }

        if (_npc_list.at(i).is_intangible() == true) {
            continue;
        }


        npc_rect = _npc_list.at(i).get_hitbox();

        collision_detection rect_collision_obj;
        if (rect_collision_obj.rect_overlap(npc_rect, p_rect) == true) {
            return &_npc_list.at(i);
        }
    }
    return NULL;
}


// kills any NPC that touches player during player's special attack
void classMap::collision_player_special_attack(character* playerObj, const short int x_inc, const short int y_inc, short int reduce_x, short int reduce_y)
{
    UNUSED(x_inc);
    UNUSED(y_inc);
    struct st_rectangle p_rect, npc_rect;
    std::vector<classnpc*>::iterator npc_it;

    //reduce = abs((float)16-playerObj->sprite->w)*0.5;

    // ponto 3, topo/esquerda
    if (playerObj->get_direction() == ANIM_DIRECTION_LEFT) {
        p_rect.x = playerObj->getPosition().x + reduce_x;
        p_rect.w = playerObj->get_size().width;
    } else {
        p_rect.x = playerObj->getPosition().x;
        p_rect.w = playerObj->get_size().width - reduce_x;
    }
    p_rect.y = playerObj->getPosition().y + reduce_y;
    p_rect.h = playerObj->get_size().height;

    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_player_friend() == true) {
            continue;
        }
        if (_npc_list.at(i).is_dead() == true) {
            continue;
        }
        if (_npc_list.at(i).is_invisible() == true) {
            continue;
        }

        if (_npc_list.at(i).is_on_visible_screen() == false) {
            continue;
        }


        npc_rect.x = _npc_list.at(i).getPosition().x;
        npc_rect.w = _npc_list.at(i).get_size().width;
        npc_rect.y = _npc_list.at(i).getPosition().y;
        npc_rect.h = _npc_list.at(i).get_size().height;

        if (_npc_list.at(i).get_size().width >= TILESIZE) { // why is this here??? O.o
            npc_rect.x = _npc_list.at(i).getPosition().x+PLAYER_NPC_COLLISION_REDUTOR;
            npc_rect.w = _npc_list.at(i).get_size().width-PLAYER_NPC_COLLISION_REDUTOR;
        }
        if (_npc_list.at(i).get_size().height >= TILESIZE) {
            npc_rect.y = _npc_list.at(i).getPosition().y+PLAYER_NPC_COLLISION_REDUTOR;
            npc_rect.h = _npc_list.at(i).get_size().height-PLAYER_NPC_COLLISION_REDUTOR;
        }
        collision_detection rect_collision_obj;
        if (rect_collision_obj.rect_overlap(npc_rect, p_rect) == true) {
            _npc_list.at(i).damage(12, false);
        }
    }
}

classnpc* classMap::find_nearest_npc(st_position pos)
{
    int min_dist = 9999;
    classnpc* min_dist_npc = NULL;

    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_player_friend() == true) {
            //std::cout << "collision_player_npcs - FRIEND" << std::endl;
            continue;
        }
        if (_npc_list.at(i).is_dead() == true) {
            //std::cout << "collision_player_npcs - DEAD" << std::endl;
            continue;
        }
        if (_npc_list.at(i).is_invisible() == true) {
            //std::cout << "collision_player_npcs - INVISIBLE" << std::endl;
            continue;
        }
        if (_npc_list.at(i).is_on_visible_screen() == false) {
            continue;
        }
        float dist = sqrt(pow((pos.x - _npc_list.at(i).getPosition().x), 2) + pow((pos.y - _npc_list.at(i).getPosition().y), 2));
        if (dist < min_dist) {
            min_dist_npc = &_npc_list.at(i);
            min_dist = dist;
        }
    }
    return min_dist_npc;
}

classnpc *classMap::find_nearest_npc_on_direction(st_position pos, int direction)
{
    int lower_dist = 9999;
    classnpc* ret = NULL;

    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_on_visible_screen() == false) {
            continue;
        }
        if (_npc_list.at(i).is_dead() == true) {
            continue;
        }

        st_position npc_pos(_npc_list.at(i).getPosition().x*TILESIZE, _npc_list.at(i).getPosition().y*TILESIZE);
        npc_pos.x = (npc_pos.x + _npc_list.at(i).get_size().width/2)/TILESIZE;
        npc_pos.y = (npc_pos.y + _npc_list.at(i).get_size().height)/TILESIZE;

        // if facing left, ignore enemies with X greater than player x
        if (direction == ANIM_DIRECTION_LEFT && npc_pos.x > pos.x) {
            continue;
        }
        // if facing right, ignore enemies with X smaller than player x+width
        if (direction == ANIM_DIRECTION_RIGHT && npc_pos.x < pos.x) {
            continue;
        }

        // pitagoras: raiz[ (x2-x1)^2 + (y2-y1)^2 ]
        int dist = sqrt(pow((float)(pos.x - npc_pos.x), (float)2) + pow((float)(pos.y - npc_pos.y ), (float)2));
        if (dist < lower_dist) {
            lower_dist = dist;
            ret = &_npc_list.at(i);
        }
    }
    return ret;
}

/// @TODO: fix animation. investigate a better way for drawing it (code is way too confusing)
void classMap::redraw_boss_door(bool is_close, int nTiles, int tileX, int tileY, short player_number) {
	//is_close = false; // THIS IS A TEMPORARY FIX

	//std::cout << "classMap::redraw_boss_door - is_close: " << is_close << std::endl;

    timer.delay(10);
	for (int k=0; k<nTiles; k++) {
		//if (is_close == false) { std::cout << "classMap::redraw_boss_door - nTiles: " << nTiles << ", tilePieces: " << tilePieces << ", tileCount: " << tileCount << std::endl; }
		// redraw screen
		showMap();

        _3rd_level_ignore_area = st_rectangle(tileX, tileY-5, 1, nTiles+5);
        showAbove();
        draw_lib.update_screen();
		int tiles_showed;
		if (is_close == false) {
			tiles_showed = k;
		} else {
			tiles_showed = 0;
		}
		for (int i=0; i<MAP_W; i++) {
			for (int j=0; j<MAP_H; j++) {
                if (map_data[number].tiles[i][j].tile3.x != -1 && map_data[number].tiles[i][j].tile3.y != -1) {
                        if (i == tileX && map_data[number].tiles[i][j].locked == TERRAIN_DOOR) {
							//std::cout << "****** redraw_boss_door - k: " << k << ", tiles_showed: " << tiles_showed << ", nTiles: " << nTiles << std::endl;
							if (is_close == false) {
								if (tiles_showed < nTiles) {

                                    if (!graphLib.gameScreen.get_surface()) {
                                        graphLib.show_debug_msg("EXIT #21.C");
                                        SDL_Quit();
                                        exit(-1);
                                    }


                                    graphLib.placeTile(st_position(map_data[number].tiles[i][j].tile3.x, map_data[number].tiles[i][j].tile3.y), st_position((i*TILESIZE)-scroll.x, (j*TILESIZE)-scroll.y), &graphLib.gameScreen);
                                    draw_lib.update_screen();
									tiles_showed++;
								}
							} else {
								if (tiles_showed < k) {

                                    if (!graphLib.gameScreen.get_surface()) {
                                        graphLib.show_debug_msg("EXIT #21.D");
                                        SDL_Quit();
                                        exit(-1);
                                    }


                                    graphLib.placeTile(st_position(map_data[number].tiles[i][j].tile3.x, map_data[number].tiles[i][j].tile3.y), st_position((i*TILESIZE)-scroll.x, (j*TILESIZE)-scroll.y), &graphLib.gameScreen);
                                    draw_lib.update_screen();
									tiles_showed++;
								}
							}
						} else {

                            if (!graphLib.gameScreen.get_surface()) {
                                graphLib.show_debug_msg("EXIT #21.E");
                                SDL_Quit();
                                exit(-1);
                            }


                            graphLib.placeTile(st_position(map_data[number].tiles[i][j].tile3.x, map_data[number].tiles[i][j].tile3.y), st_position((i*TILESIZE)+scroll.x, (j*TILESIZE)-scroll.y), &graphLib.gameScreen);
						}
				}
			}
		}
        _player_ref->show();
        graphLib.draw_hp_bar(_player_ref->get_current_hp(), player_number, WEAPON_DEFAULT, fio.get_heart_pieces_number(game_save));
        showAbove();
        draw_lib.update_screen();
        timer.delay(100);
	}
    if (is_close == true) {
        _3rd_level_ignore_area = st_rectangle(-1, -1, -1, -1);
        showAbove();
        draw_lib.update_screen();
    }
    timer.delay(100);
}


void classMap::add_animation(ANIMATION_TYPES pos_type, graphicsLib_gSurface* surface, const st_float_position &pos, st_position adjust_pos, unsigned int frame_time, unsigned int repeat_times, int direction, st_size framesize)
{
    //std::cout << ">>>>> classMap::add_animation - repeat_times: " << repeat_times << std::endl;
    animation_list.push_back(animation(pos_type, surface, pos, adjust_pos, frame_time, repeat_times, direction, framesize, &scroll));
}

void classMap::add_animation(animation anim)
{
    animation_list.push_back(anim);
}

void classMap::clear_animations()
{
	animation_list.erase(animation_list.begin(), animation_list.end());
}

void classMap::set_player(classPlayer *player_ref)
{
    _player_ref = player_ref;
}

classnpc* classMap::spawn_map_npc(short npc_id, st_position npc_pos, short int direction, bool player_friend, bool progressive_span)
{

    gameControl.must_break_npc_loop = true;

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "###ROCKBOT2###", "MAP::spawn_map_npc, id[%d]", npc_id);
#endif

    classnpc new_npc(stage_number, number, npc_id, npc_pos, direction, player_friend);

    if (progressive_span == true) {
        new_npc.set_progressive_appear_pos(new_npc.get_size().height);
    }
    _npc_list.push_back(new_npc); // insert new npc at the list-end

    classnpc* npc_ref = &(_npc_list.back());

    int id = npc_ref->get_number();
    std::string npc_name = npc_ref->get_name();

    gameControl.must_break_npc_loop = true;

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "###ROCKBOT2###", "MAP::spawn_map_npc, name[%s], must_break_loop[%d]", npc_name.c_str(), gameControl.must_break_npc_loop?1:0);
#endif


    return npc_ref;
}


void classMap::move_npcs() /// @TODO - check out of screen
{
    //std::cout << "*************** classMap::showMap - npc_list.size: " << _npc_list.size() << std::endl;

    for (int i=0; i<_npc_list.size(); i++) {

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "###ROCKBOT2###", "MAP::move_npcs - execute #[%d]", i);
#endif

        if (gameControl.must_break_npc_loop == true) {
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "###ROCKBOT2###", ">>>>>>>>>>>>>>>>>>> MAP::move_npcs - interrupt #1");
#endif
            gameControl.must_break_npc_loop = false;
            return;
        }
        // check if NPC is outside the visible area
        st_position npc_pos = _npc_list.at(i).get_real_position();
        short dead_state = _npc_list.at(i).get_dead_state();

        std::string name(_npc_list.at(i).get_name());


        if (_npc_list.at(i).is_on_screen() != true) {
            if (dead_state == 2 && _npc_list.at(i).is_boss() == false && _npc_list.at(i).is_subboss()) {
                _npc_list.at(i).revive();
            }
            continue; // no need for moving NPCs that are out of sight
        } else if (dead_state == 2 && _npc_list.at(i).auto_respawn() == true && _npc_list.at(i).is_boss() == false) {
            _npc_list.at(i).reset_position();
            _npc_list.at(i).revive();
            continue;
        } else if (dead_state == 1 && _npc_list.at(i).is_spawn() == false && _npc_list.at(i).is_boss() == false) {// drop item
            drop_item(i);
        }

        // if is showing stage boss on a stage already finished, just teleport out, victory is yours!
        if (_npc_list.at(i).is_stage_boss() == true && _npc_list.at(i).is_on_visible_screen() == true && game_save.stages[gameControl.currentStage] == 1 && gameControl.currentStage <= 8) {
            gameControl.got_weapon();
            return;
        }

        _npc_list.at(i).execute(); // TODO: must pass scroll map to npcs somwhow...




		if (dead_state == 1) {
            if (_npc_list.at(i).is_stage_boss() == false) {
                _npc_list.at(i).execute_ai(); // to ensure death-reaction is run

                // sub-boss have a different explosion
                if (_npc_list.at(i).is_subboss()) {
                    soundManager.play_repeated_sfx(SFX_BIG_EXPLOSION, 1);
                    st_float_position pos1(_npc_list.at(i).getPosition().x+2, _npc_list.at(i).getPosition().y+20);
                    add_animation(ANIMATION_STATIC, &graphLib.bomb_explosion_surface, pos1, st_position(-8, -8), 80, 2, _npc_list.at(i).get_direction(), st_size(56, 56));
                    st_float_position pos2(pos1.x+50, pos1.y-30);
                    add_animation(ANIMATION_STATIC, &graphLib.bomb_explosion_surface, pos2, st_position(-8, -8), 80, 2, _npc_list.at(i).get_direction(), st_size(56, 56));
                } else if (_npc_list.at(i).getPosition().y < RES_H) { // don't add death explosion when dying out of screen
                    add_animation(ANIMATION_STATIC, &graphLib.explosion32, _npc_list.at(i).getPosition(), st_position(-8, -8), 80, 2, _npc_list.at(i).get_direction(), st_size(32, 32));
                }
                // check if boss flag wasn't passed to a spawn on dying reaction AI
                if (_npc_list.at(i).is_boss()) {
                    gameControl.check_player_return_teleport();
                }
                // all kinds of bosses need to remove projectiles once dying
                if (_npc_list.at(i).is_boss() || _npc_list.at(i).is_subboss() || _npc_list.at(i).is_stage_boss()) {
                    _npc_list.at(i).clean_projectiles();
                // regular enemies only remove effect-type projectiles (quake, wind, freeze, etc)
                } else {
                    _npc_list.at(i).clean_effect_projectiles();
                }
            } else {

                //std::cout << "##### STAGE-BOSS IS DEAD (#1) #####" << std::endl;

                // run npc move one more time, so reaction is executed to test if it will spawn a new boss (replace-itself)
                _npc_list.at(i).execute_ai(); // to ensure death-reaction is run
                _npc_list.at(i).execute_ai(); // to ensure death-reaction is run



                if (_npc_list.at(i).is_stage_boss() == false) { // if now the NPC is not the stage boss anymore, continue
                    //std::cout << "##### STAGE-BOSS IS DEAD (#2) #####" << std::endl;
                    gameControl.draw_explosion(npc_pos.x, npc_pos.y, true);
                    soundManager.play_boss_music();
                    graphLib.blink_screen(255, 255, 255);
                    gameControl.fill_boss_hp_bar();
                    continue;
                } else {
                    //std::cout << "##### STAGE-BOSS IS DEAD (#3) #####" << std::endl;
                    gameControl.remove_all_projectiles();
                    //std::cout << "classMap::showMap - killed stage boss" << std::endl;
                    graphLib.set_screen_adjust(st_position(0, 0));
                    /// @TODO - replace with game_data.final_boss_id
                    if (game_data.final_boss_id == _npc_list.at(i).get_number()) {
                        gameControl.show_ending();
                        return;
                    } else {
                        gameControl.draw_explosion(npc_pos.x, npc_pos.y, true);
                        gameControl.got_weapon();
                    }
                }
			}
			return;
		}

        if (gameControl.must_break_npc_loop == true) {
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "###ROCKBOT2###", ">>>>>>>>>>>>>> MAP::move_npcs - interrupt #2");
#endif
            gameControl.must_break_npc_loop = false;
            return;
        }

    }
}

void classMap::show_npcs() /// @TODO - check out of screen
{
    for (int i=0; i<_npc_list.size(); i++) {
        if (gameControl.must_show_boss_hp() && _npc_list.at(i).is_boss() && _npc_list.at(i).is_on_visible_screen() == true) {
            graphLib.draw_hp_bar(_npc_list.at(i).get_current_hp(), -1, -1, BOSS_INITIAL_HP);
		}
        if (_npc_list.at(i).is_dead() == false) {
            _npc_list.at(i).show();
        }
    }
}

void classMap::move_objects(bool paused)
{
    /// @TODO - update timers
    std::vector<object>::iterator object_it;
	for (object_it = object_list.begin(); object_it != object_list.end(); object_it++) {
		if ((*object_it).finished() == true) {
			object_list.erase(object_it);
			break;
		} else {
            (*object_it).execute(paused); /// @TODO: must pass scroll map to npcs somwhow...
		}
    }
}

std::vector<object*> classMap::check_collision_with_objects(st_rectangle collision_area)
{
    std::vector<object*> res;

    for (unsigned int i=0; i<object_list.size(); i++) {
        object* temp_obj = &object_list.at(i);
        collision_detection rect_collision_obj;
        bool res_collision = rect_collision_obj.rect_overlap(temp_obj->get_area(), collision_area);
        if (res_collision == true) {
            res.push_back(temp_obj);
        }
    }
    return res;
}

void classMap::show_objects(int adjust_y, int adjust_x)
{
    /// @TODO - update timers
    std::vector<object>::iterator object_it;
    for (object_it = object_list.begin(); object_it != object_list.end(); object_it++) {
        (*object_it).show(adjust_y, adjust_x); // TODO: must pass scroll map to objects somwhow...
    }
}

bool classMap::boss_hit_ground()
{
    for (int i=0; i<_npc_list.size(); i++) {
        if (_npc_list.at(i).is_boss() == true && _npc_list.at(i).is_on_visible_screen() == true) {
            //std::cout << "MAP::boss_hit_ground - move boss to ground - pos.y: " << _npc_list.at(i).getPosition().y << std::endl;

            int limit_y = _npc_list.at(i).get_start_position().y - TILESIZE;
            //std::cout << "#### limit_y: " << limit_y << std::endl;
            if (limit_y > RES_H/2) {
                limit_y = RES_H/2;
            }

            if (_npc_list.at(i).getPosition().y >= limit_y && _npc_list.at(i).hit_ground()) {
                _npc_list.at(i).set_animation_type(ANIM_TYPE_STAND);
                //std::cout << "boss_hit_ground #2" << std::endl;
                return true;
            }
			break;
		}
    }
	return false;
}

void classMap::reset_map_npcs()
{
	load_map_npcs();
}







