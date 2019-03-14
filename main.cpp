#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Surface.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>
#include <ZL_Scene.h>
#include <ZL_Data.h>
#include <ZL_Input.h>
#include <vector>

static int map_width, map_height;
static std::vector<int> map;
static ZL_Surface srfOverworld;
static ZL_Surface srfUnits;

struct Unit
{
	ZL_Vector pos, target, dir;

	void Update()
	{
		if (pos == target) return;
		float moveDist = ZLELAPSEDF(10);
		if (pos.Near(target, moveDist * 1.1f)) { pos = target; return; }
		dir = ZLV(ZL_Math::Sign0(target.x - pos.x, moveDist*.5f), ZL_Math::Sign0(target.y - pos.y, moveDist*.5f)).VecNorm();
		pos += dir * moveDist;
	}

	void Draw()
	{
		bool flip = dir.x < 0;
		int tileIndex = (dir.y < -.75f ? 0 : (dir.y < -.25f ? 1 : (dir.y < .25f ? 2 : (dir.y < .75f ? 3 : 4))));
		ZL_Display::FillCircle(pos, 1, ZL_Color::Blue);
		ZL_Display::FillCircle(target, 1, ZL_Color::Green);
		srfUnits.SetTilesetIndex(tileIndex).Draw(pos, (flip ? -1.f : 1.f)/16.f, 1.f/16.f); 
	}
};

static Unit player;
static float screen_zoom;
static ZL_Rectf screen_view, screen_display, map_area;

static void LoadMap(const char* mapfile)
{
	ZL_Xml xml((ZL_File)mapfile);
	map_width = (int)xml["width"], map_height = (int)xml["height"];
	for (ZL_Xml& layer : xml.GetChildrenByName("layer"))
	{
		ZL_ASSERT((int)layer["width"] == map_width && (int)layer["height"] == map_height);
		const ZL_Xml& data = layer("data");
		ZL_ASSERT(data["encoding"] == "base64" && data["compression"] == "zlib");
		std::vector<unsigned char> data_compressed, data_decompressed;
		ZL_Base64::Decode(data.GetText(), data_compressed);
		ZL_Compression::Decompress(data_compressed, data_decompressed);
		int* layer_data_int = (int*)&data_decompressed[0];
		map.insert(map.end(), layer_data_int, layer_data_int + (map_width*map_height));
	}
}

static void Load()
{
	LoadMap("Data/testmap.tmx");
	srfOverworld = ZL_Surface("Data/overworld.png").SetTilesetClipping(32, 32).SetScale(1/16.f);
	srfUnits = ZL_Surface("Data/units.png").SetTilesetClipping(8, 8).SetScale(1/16.f).SetOrigin(ZL_Origin::Custom(.5f, .8f));
}

static void Init()
{
	map_area = ZL_Rectf(-.5f, -.5f, map_width - .5f, map_height -.5f);
	screen_display = ZL_Rectf(0, 0, ZLWIDTH, ZLHEIGHT);
	screen_zoom = 64 / ZLWIDTH;
	screen_view = ZL_Rectf(0, 0, ZLWIDTH * screen_zoom, ZLHEIGHT * screen_zoom);
	player.pos = player.target = ZLV(10, 10);
	player.dir = ZL_Vector::Right;
}

static void Update()
{
	if (ZL_Input::Down(ZLK_ESCAPE)) { ZL_Application::Quit(); return; }

	ZL_Vector playerMove((ZL_Input::Down(ZLK_LEFT) || ZL_Input::Down(ZLK_A) ? -1.f : 0.f)+(ZL_Input::Down(ZLK_RIGHT) || ZL_Input::Down(ZLK_D) ?  1.f : 0.f),
	                     (ZL_Input::Down(ZLK_DOWN) || ZL_Input::Down(ZLK_S) ? -1.f : 0.f)+(ZL_Input::Down(ZLK_UP)    || ZL_Input::Down(ZLK_W) ?  1.f : 0.f));
	player.target += playerMove;

	ZL_Vector pointer = ZL_Input::Pointer();
	if (ZL_Input::Down(ZL_BUTTON_LEFT))
	{

		player.target = ZL_Rectf::Map(pointer, screen_display, screen_view);
		player.target.x = (float)(int)(player.target.x + .5f);
		player.target.y = (float)(int)(player.target.y + .5f);
	}

	ZL_Vector scroll(pointer.x < 60 ? -1.f : (pointer.x > ZLFROMW(60) ? 1.f : 0), (pointer.y < 60 ? -1.f : (pointer.y > ZLFROMH(60) ? 1.f : 0)));
	if (ZL_Input::Held(ZL_BUTTON_MIDDLE)) scroll = -ZL_Input::PointerDelta()*screen_zoom;
	if (!!scroll)
	{
		screen_view += scroll;
		if (screen_view.left  < map_area.left ) screen_view.right = (screen_view.left  = map_area.left ) + ZLWIDTH  * screen_zoom;
		if (screen_view.low   < map_area.low  ) screen_view.high  = (screen_view.low   = map_area.low  ) + ZLHEIGHT * screen_zoom;
		if (screen_view.right > map_area.right) screen_view.left  = (screen_view.right = map_area.right) - ZLWIDTH  * screen_zoom;
		if (screen_view.high  > map_area.high ) screen_view.low   = (screen_view.high  = map_area.high ) - ZLHEIGHT * screen_zoom;
	}

	player.Update();
}

static void Draw()
{
	ZL_Display::PushOrtho(screen_view);

	//draw map tiles
	ZL_Rectf draw_area((int)(screen_view.left - .5f) - .5f, (int)(screen_view.low - .5f) - .5f, screen_view.right, screen_view.high);
	for (int layer = 0; layer != 2; layer++)
	{
		for (float y = draw_area.low; y < draw_area.high; y++)
		{
			int* row = &map[((layer * map_height) + (map_height - 1 - (int)(y + .6f))) * map_width + (int)(draw_area.left + .6f)];
			for (float x = draw_area.left; x < draw_area.right; x++, row++)
			{
				if (!*row) continue;
				srfOverworld.SetTilesetIndex(*row-1).Draw(x, y);
			}
		}
	}

	//draw map grid
	for (float y = draw_area.low;  y < draw_area.high;  y++) ZL_Display::DrawLine(map_area.left, y, map_area.right, y, ZLRGBA(1,1,0,.2));
	for (float x = draw_area.left; x < draw_area.right; x++) ZL_Display::DrawLine(x,  map_area.low, x,  map_area.high, ZLRGBA(1,1,0,.2));

	//draw units
	player.Draw();

	ZL_Display::PopOrtho();
}

static struct sCommandzilla : public ZL_Application
{
	sCommandzilla() : ZL_Application(60) { }

	virtual void Load(int argc, char *argv[])
	{
		if (!ZL_Application::LoadReleaseDesktopDataBundle()) return;
		if (!ZL_Display::Init("Commandzilla", 1600, 900, ZL_DISPLAY_ALLOWRESIZEHORIZONTAL)) return;
		ZL_Display::ClearFill(ZL_Color::White);
		ZL_Display::SetAA(true);
		ZL_Audio::Init();
		ZL_Input::Init();
		::Load();
		::Init();
	}

	virtual void AfterFrame()
	{
		::Update();
		::Draw();
	}
} Commandzilla;
