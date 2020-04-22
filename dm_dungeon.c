#include <stdlib.h>
#include <math.h>
#include <stdio.h> // TODO REMOVE
#include "dm_algorithm.h"
#include "dm_dungeon.h"

///////////////////////////
// CELLMAP BUILDERS

struct tunnel_dir tunnel_dirs[] = {
	// right, down, left, up
	{1,0}, {0,1}, {-1,0}, {0,-1}
};

///////////////////////////
// GROUND START

void dng_cellmap_buildground(struct dng_cellmap *cellmap)
{
	cellmap->cells = (struct dng_cell**)malloc(cellmap->size * sizeof(struct dng_cell*));
	cellmap->cells_length = cellmap->size; // redundant but consistent

	for (int i=0; i < cellmap->size; i++) {
		struct dng_cell* cell = (struct dng_cell*)malloc(sizeof(struct dng_cell));
		dng_cell_init(cell);

		// TODO is this why it blows up with non-square maps?
		cell->index = i;
		cell->x = i % cellmap->width;
		cell->y = i / cellmap->height;

		cellmap->cells[i] = cell;
	}
}

void dng_cell_init(struct dng_cell *cell)
{
	cell->index = 0;
	cell->x = 0;
	cell->y = 0;
	cell->is_wall = false;
	cell->is_entrance = false;

	cell->room = NULL;
	cell->is_room_edge = false;
	cell->is_room_perimeter = false;

        cell->is_tunnel = false;
        cell->was_corridor_tunnel = false;
        cell->was_door_tunnel = false;
        cell->was_room_fix_tunnel = false;

	cell->sill_data.door_x = -1;
	cell->sill_data.door_y = -1;
	cell->is_sill = false;
        cell->is_door = false;
        cell->was_door = false;
	cell->door; // TODO? is this ok?
}

// GROUND END
///////////////////////////



///////////////////////////
// ROOMS START

void dng_cellmap_buildrooms(struct dng_cellmap *cellmap)
{
	cellmap->rooms = (struct dng_room**)malloc(cellmap->room_count * sizeof(struct dng_room*));
	int i=0;
	cellmap->rooms_length = 0;
	for (int a = 0; a < cellmap->room_attempt_count; a++) {
		struct dng_room* room = dng_cellmap_buildroom(cellmap);
		if (room) {
			cellmap->rooms[i] = room;
			cellmap->rooms_length++;
			i++;
		}
		if (cellmap->rooms_length > cellmap->room_count)
			return;
	}
}

struct dng_room* dng_cellmap_buildroom(struct dng_cellmap *cellmap)
{
	int room_width = dm_randii(cellmap->min_room_width, cellmap->max_room_width);
	int room_height = dm_randii(cellmap->min_room_height, cellmap->max_room_height);
	struct dng_room *room = NULL;
	int i=0;

	// sample cells spread out by room scatter factor
	bool inspect_sp(struct dng_cell* cell) {
		if (i % cellmap->room_scatter == 0) {
			// if the map can house this room then lets pick it for a room spot
			if (dng_cellmap_can_house_dimension(cellmap, cell->x, cell->y, room_width, room_height)) {

				// can be a room if not already a room or room perimeter, if any cell then no
				bool can_be_placed = true;
				bool inspect_dim(struct dng_cell* cell) {
					if (cell->room != NULL || cell->is_room_perimeter) {
						can_be_placed = false;
						return true;
					}
					return false;
				}
				dng_cellmap_inspect_cells_in_dimension(cellmap, cell->x, cell->y, room_width, room_height, inspect_dim);

				if (can_be_placed) {
					room = (struct dng_room*)malloc(sizeof(struct dng_room));
					dng_room_init(room, cell->x, cell->y, room_width, room_height);
					dng_cellmap_emplace_room(cellmap, room);
					return true;
				}
			}
		}
		i++;
		return false;
	}

	dng_cellmap_inspect_spiral_cells(cellmap, inspect_sp);

	return room;
}

void dng_room_init(struct dng_room *room, int x, int y, int w, int h)
{
	//: x(0), y(0), width(0), height(0), entranceWeight(0), isMachineRoom(false)
	room->x = x;
	room->y = y;
	room->width = w;
	room->height = h;
	room->entrance_weight = 0;
	room->is_machine_room = false;
}

void dng_cellmap_emplace_room(struct dng_cellmap *cellmap, struct dng_room *room)
{
	bool inspect_dim(struct dng_cell* cell) {
		cell->room = room;
		// set room edge
		cell->is_room_edge = (
			cell->x == room->x ||
			cell->y == room->y ||
			cell->x == room->x + room->width - 1 ||
			cell->y == room->y + room->height - 0
		);
		// set exterior walls
		if (cell->is_room_edge) {
			// top left corner
			if (cell->x == room->x && cell->y == room->y) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x - 1, cell->y - 1);
				neighbor->is_room_perimeter = true;
			}
			// top right corner
			if (cell->x == room->x + room->width - 1 && cell->y == room->y) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x + 1, cell->y - 1);
				neighbor->is_room_perimeter = true;
			}
			// bottom left corner
			if (cell->x == room->x && cell->y == room->y + room->height - 1) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x - 1, cell->y + 1);
				neighbor->is_room_perimeter = true;
			}
			// bottom right corner
			if (cell->x == room->x && cell->y == room->y + room->height - 1) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x + 1, cell->y + 1);
				neighbor->is_room_perimeter = true;
			}
			// Left side
			if (cell->x == room->x) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x - 1, cell->y);
				neighbor->is_room_perimeter = true;
			}
			// top side
			if (cell->y == room->y) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x, cell->y - 1);
				neighbor->is_room_perimeter = true;
			}
			// right side
			if(cell->x == room->x + room->width - 1) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x + 1, cell->y);
				neighbor->is_room_perimeter = true;
			}
			// bottom side
			if (cell->y == room->y + room->height - 1) {
				struct dng_cell *neighbor = dng_cellmap_get_cell_at_position(cellmap, cell->x, cell->y + 1);
				neighbor->is_room_perimeter = true;
			}
		}
		return false;
	}

	dng_cellmap_inspect_cells_in_dimension(cellmap, room->x, room->y, room->width, room->height, inspect_dim);
}

// ROOMS END
///////////////////////////


///////////////////////////
// TUNNELS START
void dng_cellmap_buildtunnels(struct dng_cellmap *cellmap)
{
	// Iterate all cells within the map
	for (int i = cellmap->map_padding; i < cellmap->width - cellmap->map_padding; i++) { // cols
		for(int j = cellmap->map_padding; j < cellmap->height - cellmap->map_padding; j++) { // rows
			// Build recursive tunnels from cell
			struct dng_cell *cell = dng_cellmap_get_cell_at_position(cellmap, i, j);
			if (cell->room == NULL && !cell->is_room_perimeter && !cell->is_tunnel) {
				//tunnel2(cell);
				dng_cellmap_tunnel(cellmap, cell, tunnel_dirs[0]);
			}
		}
	}
}

// recursive dig
void dng_cellmap_tunnel(struct dng_cellmap *cellmap, struct dng_cell *cell, struct tunnel_dir last_dir)
{
	// ORIGINAL
	if (last_dir.x == 0 && last_dir.y == 0) {
		// first cell
		last_dir = tunnel_dirs[0];
	}

	bool attempt_straight = dm_randf() > cellmap->tunnel_turn_ratio; // % chance to try to go straight

	// create new tunnel dirs to randomize
	struct tunnel_dir rand_dirs[4];
	for (int i=0; i<4; i++)
		rand_dirs[i] = tunnel_dirs[i];
	dng_get_shuffled_directions(rand_dirs);

	// Try to dig straight
	if (attempt_straight) {
		if (dng_cellmap_open_tunnel(cellmap, cell, last_dir)) {
			struct dng_cell *next_cell = dng_cellmap_get_cell_at_position(cellmap, cell->x + last_dir.x, cell->y + last_dir.y);
			dng_cellmap_tunnel(cellmap, next_cell, last_dir);
		}
	}

	// Try to dig in any direction
	for (unsigned int i=0; i < 4; i++) {
		struct tunnel_dir dir = rand_dirs[i];

		if (attempt_straight && dir.x == last_dir.x && dir.y == last_dir.y)
			continue;

		if (dng_cellmap_open_tunnel(cellmap, cell, dir)) {
			struct dng_cell *next_cell = dng_cellmap_get_cell_at_position(cellmap, cell->x + dir.x, cell->y + dir.y);
			dng_cellmap_tunnel(cellmap, next_cell, dir);
		}
	}
}

bool dng_cellmap_open_tunnel(struct dng_cellmap *cellmap, struct dng_cell *cell, struct tunnel_dir dir)
{
    if (dng_cellmap_can_tunnel(cellmap, cell, dir)) {
        dng_cellmap_emplace_tunnel(cellmap, cell, dir);
        return true;
    }

    return false;
}

bool dng_cellmap_can_tunnel(struct dng_cellmap *cellmap, struct dng_cell *cell, struct tunnel_dir dir)
{
	int next_x = cell->x + dir.x;
	int next_y = cell->y + dir.y;
	int next_right_x = next_x + -dir.y;
	int next_right_y = next_y + dir.x;
	int next_left_x = next_x + dir.y;
	int next_left_y = next_y + -dir.x;

	int nextUpX = next_x + dir.x;
	int nextUpY = next_y + dir.y;
	int nextUpRightX = nextUpX + -dir.y;
	int nextUpRightY = nextUpY + dir.x;
	int nextUpLeftX = nextUpX + dir.y;
	int nextUpLeftY = nextUpY + -dir.x;

	if (cell->room != NULL)
		return false; // I added this in baleon code, not xogeni

	// Cell two positions over cannot be: outside of margin, room perimeter, another corridor
	if (next_x >= cellmap->map_padding && next_y >= cellmap->map_padding && next_x < cellmap->width - cellmap->map_padding && next_y < cellmap->height - cellmap->map_padding) {
		struct dng_cell *next_cell = dng_cellmap_get_cell_at_position(cellmap, next_x, next_y);
		struct dng_cell *next_left_cell = dng_cellmap_get_cell_at_position(cellmap, next_left_x, next_left_y);
		struct dng_cell *next_right_cell = dng_cellmap_get_cell_at_position(cellmap, next_right_x, next_right_y);
		struct dng_cell *next_up_cell = dng_cellmap_get_cell_at_position(cellmap, nextUpX, nextUpY);
		struct dng_cell *next_up_left_cell = dng_cellmap_get_cell_at_position(cellmap, nextUpLeftX, nextUpLeftY);
		struct dng_cell *next_up_right_cell = dng_cellmap_get_cell_at_position(cellmap, nextUpRightX, nextUpRightY);

		bool is_heading_into_room = next_cell->is_room_perimeter || next_up_cell->is_room_perimeter;
		bool is_heading_into_tunnel = next_cell->is_tunnel || next_up_cell->is_tunnel;
		bool is_running_adjacent_to_tunnel = (
			next_left_cell->is_tunnel ||
			next_right_cell->is_tunnel ||
			next_up_left_cell->is_tunnel ||
			next_up_right_cell->is_tunnel
		);

		if (!is_heading_into_room && !is_heading_into_tunnel && !is_running_adjacent_to_tunnel)
			return true;
	}

	return false;
}

void dng_cellmap_emplace_tunnel(struct dng_cellmap *cellmap, struct dng_cell *cell, struct tunnel_dir dir)
{
	struct dng_cell *next_cell = dng_cellmap_get_cell_at_position(cellmap, cell->x + dir.x, cell->y + dir.y);
	dng_cellmap_mark_as_tunnel(cellmap, cell);
}

void dng_cellmap_mark_as_tunnel(struct dng_cellmap *cellmap, struct dng_cell *cell)
{
    cell->is_tunnel = true;
    cell->was_corridor_tunnel = true;
}

void dng_get_shuffled_directions(struct tunnel_dir *dirs)
{
    int n = 4; // 4 directions
    for (int i = n - 1; i > 0; --i) {
	    dirs[i] = dirs[dm_randii(0, i + 1)];
    }
}
// TUNNELS END
///////////////////////////




///////////////////////////
// DOORS START

void dng_cellmap_builddoors(struct dng_cellmap *cellmap)
{
	for (int i=0; i < cellmap->rooms_length; i++)
		dng_cellmap_open_room(cellmap, cellmap->rooms[i]);
}

void dng_cellmap_open_room(struct dng_cellmap *cellmap, struct dng_room *room)
{
	// THIS function was pretty significantly refactored from XoGeni so
	// I could avoid these vectors beloe
	//std::vector<Cell*> sills;
	//std::vector<Cell*> doorSills;

	// Calculate number of openings this rooms could/should have
	int hyp_size = (int)sqrt(room->width * room->height);
	int num_openings = hyp_size + dm_randii(0, hyp_size);

	// assign room sills and prep a list of door sills
	int sill_length = 0;
	int starter_cell_index = -1;
	void on_sill_set(struct dng_cell *cell_sill) {
		// ensure we pick at least one cell to be a door
		// pick first, then potentially overwrite it with another random one
		if (starter_cell_index == -1 || dm_randii(0, sill_length) == sill_length)
			starter_cell_index = cell_sill->index;
		// else pick them at random to be a door cell
		else if (dm_randii(0, num_openings / 2) == 0)
			dng_cellmap_emplace_door(cellmap, room, cell_sill);

		sill_length++;
	};
	dng_cellmap_set_room_sills(cellmap, room, on_sill_set);

	// ensure we set our door for the first cell we picked
	struct dng_cell *cell_sill = cellmap->cells[starter_cell_index];
	dng_cellmap_emplace_door(cellmap, room, cell_sill);
}

void dng_cellmap_emplace_door(struct dng_cellmap* cellmap, struct dng_room *room, struct dng_cell *cell_sill)
{
	struct dng_cell *cell_door = dng_cellmap_get_cell_at_position(cellmap, cell_sill->sill_data.door_x, cell_sill->sill_data.door_y);
	if (!cell_door->is_door) {
		// TODO door types were here
		//RoomDoor::DoorType doorType;
		//switch(LevelGenerator::random.next(2))
		//{
		//	case 0:
		//		doorType = RoomDoor::DoorType::Arch;
		//		break;
		//	case 1:
		//		doorType = RoomDoor::DoorType::Door;
		//		break;
		//}

		// Create door
		int dir_x = cell_door->x - cell_sill->x;
		int dir_y = cell_door->y - cell_sill->y;
		struct dng_roomdoor roomdoor = {
			cell_door->x, cell_door->y,
			// TODO DOOR TYPE
			cell_door->room,
			dir_x, dir_y
		};

		cell_door->is_door = true;
		cell_door->was_door = true;
		cell_door->door = roomdoor;

		// Help connections
		dng_cellmap_connect_door(cellmap, &roomdoor);
	}
}

// TODO this is hardcoded for square rooms, not round or cavelikes
void dng_cellmap_set_room_sills(struct dng_cellmap* cellmap, struct dng_room *room, void (*on_set)(struct dng_cell*))
{
	// identify and mark sills of a room
	int topMargin = cellmap->min_hall_width + 2;
	int bottomMargin = cellmap->height - (cellmap->min_hall_width + 2);
	int leftMargin = cellmap->min_hall_width + 2;
	int rightMargin = cellmap->width - (cellmap->min_hall_width + 2);
	int cornerSpacing = 2; // do not let sills be valid within 2 spaces of corners

	// North wall
	if (room->y > topMargin) {
		for (int i = room->x + cornerSpacing; i < room->x + room->width - cornerSpacing; i++) {
			if(i % 2 == 0) {
				struct dng_cell* cell = dng_cellmap_get_cell_at_position(cellmap, i, room->y);
				cell->is_sill = true;
				cell->sill_data.door_x = cell->x;
				cell->sill_data.door_y = cell->y - 1;
				on_set(cell);
				//fill.push_back(cell);
			}
		}
	}
	// South wall
	if (room->y + room->height < bottomMargin) {
		for (int i = room->x + cornerSpacing; i < room->x + room->width - cornerSpacing; i++) {
			if (i % 2 == 0) {
				struct dng_cell* cell = dng_cellmap_get_cell_at_position(cellmap, i, room->y + room->height - 1);
				cell->is_sill = true;
				cell->sill_data.door_x = cell->x;
				cell->sill_data.door_y = cell->y + 1;
				on_set(cell);
				//fill.push_back(cell);
			}
		}
	}
	// East wall
	if (room->x > leftMargin) {
		for (int i = room->y + cornerSpacing; i < room->y + room->height - cornerSpacing; i++) {
			if (i % 2 == 0) {
				struct dng_cell* cell = dng_cellmap_get_cell_at_position(cellmap, room->x, i);
				cell->is_sill = true;
				cell->sill_data.door_x = cell->x - 1;
				cell->sill_data.door_y = cell->y;
				on_set(cell);
				//fill.push_back(cell);
			}
		}
	}
	// West wall
	if (room->x + room->width < rightMargin) {
		for (int i = room->y + cornerSpacing; i < room->y + room->height - cornerSpacing; i++) {
			if (i % 2 == 0) {
				struct dng_cell* cell = dng_cellmap_get_cell_at_position(cellmap, room->x + room->width - 1, i);
				cell->is_sill = true;
				cell->sill_data.door_x = cell->x + 1;
				cell->sill_data.door_y = cell->y;
				on_set(cell);
				//fill.push_back(cell);
			}
		}
	}
}

void dng_cellmap_connect_door(struct dng_cellmap *cellmap, struct dng_roomdoor *door)
{
	// Tunnel away from door until we find
	// - A tunnel
	// - Another door
	// - Another room

	bool complete = false;
	struct dng_cell* cell = dng_cellmap_get_cell_at_position(cellmap, door->x, door->y);
	while (!complete) {
		struct dng_cell* right_cell = dng_cellmap_get_cell_at_position(cellmap, cell->x - door->dir_y, cell->y + door->dir_x);
		struct dng_cell* left_cell = dng_cellmap_get_cell_at_position(cellmap, cell->x + door->dir_y, cell->y - door->dir_x);

		cell = dng_cellmap_get_cell_at_position_nullable(cellmap, cell->x + door->dir_x, cell->y + door->dir_y);
		if (cell != NULL) {
			bool stop = (
				cell->is_tunnel || cell->is_door || cell->is_room_edge || // next cell
				right_cell->is_tunnel || left_cell->is_tunnel || // side is a tunnel
				right_cell->is_room_edge || left_cell->is_room_edge || // side is a room
				right_cell->is_door || left_cell->is_door // side is a door
			);
			if (stop) {
				complete = true;
			} else {
				cell->is_tunnel = true;
				cell->was_door_tunnel = true;
			}
		} else {
			// TODO: Failure condition
			complete = true;
		}
	}
}

// DOORS END
///////////////////////////




///////////////////////////
// CELLMAP INSPECTORS START
void dng_cellmap_inspect_spiral_cells(struct dng_cellmap *cellmap, bool (*inspect)(struct dng_cell*))
{
	int center_x = cellmap->width / 2;
	int center_y = cellmap->height / 2;
	struct dm_spiral sp = dm_spiral(-1); // TODO infinie untested
	do {
		int current_x = center_x + sp.x;
		int current_y = center_y + sp.y;
		if (current_x >= cellmap->width || current_y >= cellmap->height)
			return;
		struct dng_cell *current = dng_cellmap_get_cell_at_position(cellmap, current_x, current_y);
		if (inspect(current))
			return; // break
	} while(dm_spiralnext(&sp));
}

void dng_cellmap_inspect_cells_in_dimension(struct dng_cellmap *cellmap, int x, int y, int w, int h, bool (*inspect)(struct dng_cell*))
{
	for (int i = x; i < x + w; i++) { // cols
		for (int j = y; j < y + h; j++) { // rows
			struct dng_cell *cell = dng_cellmap_get_cell_at_position(cellmap, i, j);
			if (inspect(cell)) {
				// break; // TODO should this be return?
				return;
			}
		}
	}
}

bool dng_cellmap_can_house_dimension(struct dng_cellmap *cellmap, int x, int y, int w, int h)
{
	if (x > cellmap->map_padding && y > cellmap->map_padding) {
		if (x + w + cellmap->map_padding < cellmap->width && y + h + cellmap->map_padding < cellmap->height) {
			return true;
		}
	}
	return false;
}

struct dng_cell* dng_cellmap_get_cell_at_position(struct dng_cellmap *cellmap, int x, int y)
{
	return cellmap->cells[y * cellmap->width + x];
}

struct dng_cell* dng_cellmap_get_cell_at_position_nullable(struct dng_cellmap *cellmap, int x, int y)
{
	if (x > 0 && y > 0 && x < cellmap->width && y < cellmap->height) {
		return cellmap->cells[y * cellmap->width + x];
	}
	return NULL;
}

// CELLMAP INSPECTORS END
///////////////////////////




struct dng_cellmap* dng_genmap(int difficulty, int width, int height)
{
	struct dng_cellmap *cellmap = (struct dng_cellmap*)malloc(sizeof(struct dng_cellmap));
	cellmap->id = 0; // TODO could be index in a list of generated dungeons
	cellmap->difficulty = difficulty;
	cellmap->width = width;
	cellmap->height = height;
	cellmap->size = width * height;

	// Room details
	cellmap->map_padding = 1;
	cellmap->min_room_width = 6;
	cellmap->max_room_width = 16;
	cellmap->min_room_height = 6;
	cellmap->max_room_height = 16;

	double map_hyp_size = sqrt(width * height);
	double hyp_min_room_size = sqrt(cellmap->min_room_width * cellmap->min_room_height);
	double hyp_max_room_size = sqrt(cellmap->max_room_width * cellmap->max_room_height);
	double hyp_size = (hyp_min_room_size + hyp_max_room_size) / 2;

	double room_density = .8;
	double max_rooms_per_map = (map_hyp_size / hyp_size) * (map_hyp_size / hyp_size);

	cellmap->room_count = max_rooms_per_map * room_density;
	cellmap->room_attempt_count = cellmap->room_count * 2;
	cellmap->room_scatter = hyp_size * (1 - room_density) + hyp_size / 2;
	cellmap->room_scatter = hyp_size / 2 + hyp_size * 10 * (1 - room_density); // TODO why is this overwritten?

	// Tunnel details
	cellmap->min_hall_width = 1;
	cellmap->tunnel_turn_ratio = 0;
	cellmap->deadend_ratio = 0;

	// Entrance details
	cellmap->entrance_count = 1;
	cellmap->exit_count = 1;

	//mapPadding = 1;
	//float mapHypSize = sqrtf(width * height);

	//// Room Details
	//minRoomWidth = 6;
	//maxRoomWidth = 16;
	//minRoomHeight = 6;
	//maxRoomHeight = 16;

	//float hypMinRoomSize = sqrtf(minRoomWidth * minRoomHeight);
	//float hypMaxRoomSize = sqrtf(maxRoomWidth * maxRoomHeight);
	//float hypSize = (hypMinRoomSize + hypMaxRoomSize) / 2;

	//float roomDensity = .8;
	//float maxRoomsPerMap = (mapHypSize / hypSize) * (mapHypSize / hypSize);

	////roomDensity = .5f + (roomDensity / 2);
	//roomCount = maxRoomsPerMap * roomDensity;
	//roomAttemptCount = roomCount * 2;
	//roomScatter = (hypSize * (1 - roomDensity)) + hypSize / 2;
	//roomScatter = (hypSize / 2) + (hypSize * 10 * (1 - roomDensity)) ;

	//// Tunnel details
	//minHallWidth = 1;
	//tunnelTurnRatio = 0;
	//deadEndRatio = 0;

	//tunnelDirs.push_back(sf::Vector2i(1, 0)); // right
	//tunnelDirs.push_back(sf::Vector2i(0, 1)); // down
	//tunnelDirs.push_back(sf::Vector2i(-1, 0)); // left
	//tunnelDirs.push_back(sf::Vector2i(0, -1)); // up

	//// Exit details
	//entranceCount = 1;
	//exitCount = 1;

	dng_cellmap_buildground(cellmap);
	dng_cellmap_buildrooms(cellmap);
	dng_cellmap_buildtunnels(cellmap);
	dng_cellmap_builddoors(cellmap);

	//cellMap->buildGround();
	//cellMap->buildRooms();
	//cellMap->buildTunnels();
	//cellMap->buildDoors();
	//cellMap->buildEntrance();
	//cellMap->cleanupConnections();
	//cellMap->calculateEntranceWeights();
	//cellMap->buildExit();
	//cellMap->buildWalls();
	//cellMap->buildLights();
	//cellMap->buildTags();
	//cellMap->machinate();

	return cellmap;
}

void dng_delmap(struct dng_cellmap *cellmap)
{
	for (int i=0; i < cellmap->rooms_length; i++) {
		free(cellmap->rooms[i]); // free cell
	}
	free(cellmap->rooms); // free cell list
	for (int i=0; i < cellmap->cells_length; i++) {
		free(cellmap->cells[i]); // free cell
	}
	free(cellmap->cells); // free cell list
	free(cellmap);
}
