#include "NetIO.h"
#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const int TILE_SIZE = 32; // width/height of a tile on screen (in pixels)
const int CHUNK_SIZE = 16; // width/height of a chunk (in tiles)
const int LOADED_AREA_SIZE = 8; // width/height of the loaded area (in chunks)
const int LOADED_AREA_R = LOADED_AREA_SIZE/2; // halfwidth of the loaded area (in chunks)
const int NUM_LOADED_CHUNKS = LOADED_AREA_SIZE*LOADED_AREA_SIZE; // number of chunks loaded at a given time

// tile types
const int EMPTY = 0, WALL = 1, FOOD = 2;

class perlin2d {
public:
	perlin2d(unsigned seed, int numOctaves) {
		this->seed = seed;
		srand(seed);
		if (numOctaves > 0)
			subOctave = new perlin2d((unsigned)rand(), numOctaves-1);
		else subOctave = NULL;
	}

	~perlin2d() {
		delete subOctave;
	}

	void setSeed(unsigned newSeed) {
		seed = newSeed;
		srand(seed);
		if (subOctave)
			subOctave->setSeed((unsigned)rand());
	}

	static float interpolate(float a, float b, float x) {
		float ft = x * float(M_PI);
		float f = (1 - cosf(ft)) * .5f;

		return  a*(1-f) + b*f;
	}

	float grid(int x, int y) {
		int n = seed*9001 + x + y * 57;
		n = (n<<13) ^ n;
		return (( 1.0f - ( (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f) + 1.0f)/2.0f;
	}

	float noise(float x, float y) {
		int xi = (int)x, yi = (int)y;
		float xf = x-xi, yf = y-yi;
		float v0 = interpolate(grid(xi,yi), grid(xi+1,yi), xf);
		float v1 = interpolate(grid(xi,yi+1), grid(xi+1,yi+1), xf);
		float v = interpolate(v0, v1, yf);

		if (subOctave)
			return (v + subOctave->noise(x*2, y*2)*0.3f) / 1.3f;
		else return v;
	}

	perlin2d* subOctave;
	unsigned seed;
};

// class representing a chunk
class Chunk {
public:
	Chunk() {}

	Chunk(int cx, int cy, perlin2d& perlin):cx(cx),cy(cy) {
		for (int x = 0; x < CHUNK_SIZE; x++) {
			int wx = cx*CHUNK_SIZE + x;
			for (int y = 0; y < CHUNK_SIZE; y++) {
				int wy = cy*CHUNK_SIZE + y;
				float noiseVal = fabsf(noiseAt(perlin, wx, wy)*2-1);
				tiles[x][y] = noiseVal>0.3f? WALL : perlin.grid(wx, wy)<0.006? FOOD : EMPTY;
			}
		}
	}

	static float noiseAt(perlin2d& perlin, int wx, int wy) {
		return perlin.noise(wx/10.0f, wy/10.0f);
	}

	void draw(int xOff, int yOff) {
		for (int x = 0; x < CHUNK_SIZE; x++) {
			int tx = (cx*CHUNK_SIZE + x)*TILE_SIZE + xOff;
			for (int y = 0; y < CHUNK_SIZE; y++) {
				int ty = (cy*CHUNK_SIZE + y)*TILE_SIZE + yOff;
				switch (tiles[x][y]) {
				case WALL:
					gl::color(Color(0, 0, 0.5f));
					gl::drawSolidRect(Rectf((float)tx, (float)ty, (float)tx+TILE_SIZE, (float)ty+TILE_SIZE));
					break;
				case FOOD:
					gl::color(Color(1, 0.5f, 0));
					gl::drawSolidCircle(Vec2f(tx+TILE_SIZE/2.0f, ty+TILE_SIZE/2.0f), TILE_SIZE/4.0f);
					break;
				}
			}
		}
	}

	int cx, cy;
	int tiles[CHUNK_SIZE][CHUNK_SIZE];
};

class BubbleFishApp;
BubbleFishApp* bfApp;

// class representing an entity
class Entity {
public:
	Entity() {}

	Entity(float x, float y, float rx, float ry):x(x),y(y),lastX(x),lastY(y),rx(rx),ry(ry),sx(0),sy(0) {

	}

	void update() {
		x += sx;
		y += sy;

		doCollide();
	}

	void doCollide();
	bool isColliding();

	float x, y, sx, sy;
	float rx, ry, lastX, lastY;
};

void Entity::doCollide() {
    float newX = x, newY = y;
    x = lastX; y = lastY;
    
    if (isColliding()) {
        x = newX;
        y = newY;
    } else {
        x = newX;
        if (isColliding()) {
			sx = 0;
            if (newX-lastX < 0)
                x = ceilf(x-rx)+rx;
            else
                x = floorf(x+rx)-rx;
        }
        
        y = newY;
        if (isColliding()) {
			sy = 0;
            if (newY-lastY < 0)
                y = ceilf(y-ry)+ry;
            else
                y = floorf(y+ry)-ry;
        }
    }
    lastX = x;
    lastY = y;
}


class BubbleFishApp : public AppNative {
public:
	BubbleFishApp():perlin((unsigned)time(NULL), 4) {
		chunks = new Chunk[NUM_LOADED_CHUNKS];
		newChunks = new Chunk[NUM_LOADED_CHUNKS];
	}
	~BubbleFishApp() {
		delete[] chunks;
		delete[] newChunks;
	}

	void setup();
	void shutdown();
	void mouseDown(MouseEvent event);
	void keyDown(KeyEvent event);
	void keyUp(KeyEvent event);
	void update();
	void updateChunkList();
	void draw();
	void writePlayer();
	void readPlayer2();
	
	bool isBlockSolid(int x, int y);
	int getBlock(int x, int y);
	void setBlock(int x, int y, int block);
	Chunk* getChunk(int cx, int cy);

	Chunk* chunks;
	Chunk* newChunks;
	perlin2d perlin;

	static const int NUM_PARTICLES = 100;
	struct particle {
		float x, y, sx, sy;
		Color col;
		int age;
	};
	particle particles[NUM_PARTICLES];
	void addParticle(float x, float y, float sx, float sy, Color col, int age);
	int bubbleTimer, bubbleTimer2;

	Entity player, player2;
	float playerFood;
	bool W, A, S, D;

	int state;
};

bool Entity::isColliding() {
    int xMin = int(x-rx);
    int xMax = int(x+rx);
    int yMin = int(y-ry);
    int yMax = int(y+ry);
	bool colliding = false;
    for (int x = xMin; x <= xMax; x++) {
        for (int y = yMin; y <= yMax; y++) {
			if (this==&bfApp->player || this==&bfApp->player2) {
				int block = bfApp->getBlock(x, y);
				if (block == FOOD &&
					fabsf(this->x-(x+0.5f)) < rx+0.249f && fabsf(this->y-(y+0.5f)) < ry+0.249f) {
					if (this==&bfApp->player)
						bfApp->playerFood += 10;
					bfApp->setBlock(x, y, EMPTY);
					continue;
				}
			}
            if (!colliding &&
				bfApp->isBlockSolid(x, y) &&
				fabsf(this->x-(x+0.5f)) < rx+0.499f && fabsf(this->y-(y+0.5f)) < ry+0.499f)
                 colliding = true;
        }
    }
    return colliding;
}

float maxf(float a, float b) {
	return a>b? a : b;
}

void BubbleFishApp::setup() {
	setWindowSize(Vec2i(1510, 750));
	setWindowPos(Vec2i(10, 40));
	
	bfApp = this;

	int i = 0;
	for (int cx = 0; cx < LOADED_AREA_SIZE; cx++) {
		for (int cy = 0; cy < LOADED_AREA_SIZE; cy++) {
			chunks[i++] = Chunk(cx, cy, perlin);
		}
	}

	for (i = 0; i < NUM_PARTICLES; i++)
		particles[i].age = 0;
	bubbleTimer = 0;

	// find a local maxima for the player
	int x, y;
	srand((unsigned)time(NULL));
	player = Entity(0, 0, 0.4f, 0.4f);
	player2 = Entity(0, 0, 0.4f, 0.4f);
	do {
		player.x = x = rand();
		player.y = y = rand();
		updateChunkList();
		float cur;
		while (true) {
			cur = Chunk::noiseAt(perlin, x, y);
			float xp = Chunk::noiseAt(perlin, x+1, y);
			float xn = Chunk::noiseAt(perlin, x-1, y);
			float yp = Chunk::noiseAt(perlin, x, y+1);
			float yn = Chunk::noiseAt(perlin, x, y-1);
			float max = maxf(cur, maxf(maxf(xp, xn), maxf(yp, yn)));
			if (max == xp) x++;
			if (max == xn) x--;
			if (max == yp) y++;
			if (max == yn) y--;
			if (max == cur) break;
		}
	} while (isBlockSolid(x, y));
	player.x = x+0.5f;
	player.y = y+0.5f;
	playerFood = 100;
	W = A = S = D = false;

	state = 0;
}

void BubbleFishApp::shutdown() {
	netio::stop();
}

void BubbleFishApp::mouseDown(MouseEvent event) {
	
}

void BubbleFishApp::writePlayer() {
	netio::writeFloat(player.x);
	netio::writeFloat(player.y);
	netio::writeFloat(player.sx);
	netio::writeFloat(player.sy);
}

void BubbleFishApp::readPlayer2() {
	player2.lastX = player2.x = netio::readFloat();
	player2.lastY = player2.y = netio::readFloat();
	player2.sx = netio::readFloat();
	player2.sy = netio::readFloat();
}

string serverAddr = "";

void BubbleFishApp::keyDown(KeyEvent event) {
	if (event.getCode() == KeyEvent::KEY_w)
		W = true;
	if (event.getCode() == KeyEvent::KEY_a)
		A = true;
	if (event.getCode() == KeyEvent::KEY_s)
		S = true;
	if (event.getCode() == KeyEvent::KEY_d)
		D = true;
	
	if (event.getCode() == KeyEvent::KEY_h && state == 0)
		state = 2;
	if (event.getCode() == KeyEvent::KEY_j && state == 0)
		state = 1;
	if (state == 1) {
		if (event.getCode() == KeyEvent::KEY_1)
			serverAddr += "1";
		if (event.getCode() == KeyEvent::KEY_2)
			serverAddr += "2";
		if (event.getCode() == KeyEvent::KEY_3)
			serverAddr += "3";
		if (event.getCode() == KeyEvent::KEY_4)
			serverAddr += "4";
		if (event.getCode() == KeyEvent::KEY_5)
			serverAddr += "5";
		if (event.getCode() == KeyEvent::KEY_6)
			serverAddr += "6";
		if (event.getCode() == KeyEvent::KEY_7)
			serverAddr += "7";
		if (event.getCode() == KeyEvent::KEY_8)
			serverAddr += "8";
		if (event.getCode() == KeyEvent::KEY_9)
			serverAddr += "9";
		if (event.getCode() == KeyEvent::KEY_0)
			serverAddr += "0";
		if (event.getCode() == KeyEvent::KEY_PERIOD)
			serverAddr += ".";
		if (event.getCode() == KeyEvent::KEY_BACKSPACE)
			serverAddr = serverAddr.substr(0, serverAddr.length()-1);
		if (event.getCode() == KeyEvent::KEY_RETURN) {
			netio::startClient(serverAddr);
			perlin.setSeed(netio::readUInt());
			player.lastX = player.x = netio::readFloat();
			player.lastY = player.y = netio::readFloat();
			// reset chunks
			for (int i = 0; i < NUM_LOADED_CHUNKS; i++) {
				chunks[i] = Chunk(-9001, -9001, perlin);
			}
			writePlayer();
			readPlayer2();
			bubbleTimer2 = 0;
			state = 4;
		}
	}
}

void BubbleFishApp::keyUp(KeyEvent event) {
	if (event.getCode() == KeyEvent::KEY_w)
		W = false;
	if (event.getCode() == KeyEvent::KEY_a)
		A = false;
	if (event.getCode() == KeyEvent::KEY_s)
		S = false;
	if (event.getCode() == KeyEvent::KEY_d)
		D = false;
}

const float PLAYER_SPD = 0.15f;
const float PLAYER_ACL = 0.02f;
const float PLAYER_FRC = 0.98f;
int updateTimer = 0;
void BubbleFishApp::update() {
	if (state == 0 || state == 1) return;
	if (state == 2) {state = 3; return;}
	if (state == 3) {
		netio::startServer();
		netio::writeUInt(perlin.seed);
		netio::writeFloat(player.x+1);
		netio::writeFloat(player.y+1);
		writePlayer();
		readPlayer2();
		bubbleTimer2 = 0;
		state = 4;
		return;
	}


	if (netio::started && --updateTimer <= 0) {
		updateTimer = 4;
		writePlayer();
		readPlayer2();
	}
	
	player.sx *= PLAYER_FRC;
	player.sy *= PLAYER_FRC;
	player2.sx *= PLAYER_FRC;
	player2.sy *= PLAYER_FRC;
	if (W) player.sy -= PLAYER_ACL;
	if (S) player.sy += PLAYER_ACL;
	if (A) player.sx -= PLAYER_ACL;
	if (D) player.sx += PLAYER_ACL;
	float spd = sqrtf(player.sx*player.sx + player.sy*player.sy);
	if (spd > PLAYER_SPD) {
		player.sx *= PLAYER_SPD/spd;
		player.sy *= PLAYER_SPD/spd;
	}
	player.update();
	player2.update();

	// decrease food
	playerFood -= 0.08f;
	// cap food
	if (playerFood > 100) playerFood = 100;
	if (playerFood < 0) playerFood = 0; // IN THE FUTURE: DIE

	// add bubble particles
	if (bubbleTimer-- <= 0) {
		addParticle(player.x, player.y, player.sx/3, player.sy/2-0.1, Color(0.5f, 0.5f, 1.0f), 60);
		bubbleTimer = rand()%100 + 100;
	}
	if (netio::started && bubbleTimer2-- <= 0) {
		addParticle(player2.x, player2.y, player2.sx/3, player2.sy/2-0.1, Color(0.5f, 0.5f, 1.0f), 60);
		bubbleTimer2 = rand()%100 + 100;
	}
	
	// update particles
	for (int i = 0; i < NUM_PARTICLES; i++) {
		particle& p = particles[i];
		if (p.age <= 0) continue;
		p.x += p.sx;
		p.y += p.sy;
		p.age--;
		if (Entity(p.x, p.y, 5.0f/TILE_SIZE, 5.0f/TILE_SIZE).isColliding())
			p.age = 0;
	}
	
	// update loaded chunks
	updateChunkList();
}

void BubbleFishApp::addParticle(float x, float y, float sx, float sy, Color col, int age) {
	particle newP = {x, y, sx, sy, col, age};
	for (int i = 0; i < NUM_PARTICLES; i++) {
		if (particles[i].age <= 0) {
			particles[i] = newP;
			break; // returns
		}
	}
}

void BubbleFishApp::updateChunkList() {
	int cxMin = int(player.x)/CHUNK_SIZE - LOADED_AREA_R;
	int cxMax = cxMin + LOADED_AREA_SIZE;
	int cyMin = int(player.y)/CHUNK_SIZE - LOADED_AREA_R;
	int cyMax = cyMin + LOADED_AREA_SIZE;
	
	int curI = 0;
	for (int x = cxMin; x < cxMax; x++) {
		for (int y = cyMin; y < cyMax; y++) {
			bool loaded = false;
			for (int i = 0; i < NUM_LOADED_CHUNKS; i++) {
				Chunk& c = chunks[i];
				if (c.cx==x && c.cy==y) {
					newChunks[curI++] = c;
					loaded = true;
					break;
				}
			}
			if (!loaded) {
				newChunks[curI++] = Chunk(x, y, perlin);
			}
		}
	}
	// swap lists
	Chunk* tmp = chunks;
	chunks = newChunks;
	newChunks = tmp;
}

void BubbleFishApp::draw() {
	gl::clear(Color(0, 0, 1));

	float x = getWindowWidth()/2;
	float y = getWindowHeight()/2;

	if (state == 0) {
		gl::enableAlphaBlending();
		gl::drawStringCentered("Press H to host or J to join", Vec2f(x, y), ColorA(0, 0, 0, 1), Font("Arial",  50));
		gl::disableAlphaBlending();
		return;
	}
	if (state == 2 || state == 3) {
		gl::enableAlphaBlending();
		gl::drawStringCentered("Waiting for player 2...", Vec2f(x, y), ColorA(0, 0, 0, 1), Font("Arial",  50));
		gl::drawStringCentered("IP: "+netio::getLocalAddress(), Vec2f(x, y+100), ColorA(0, 0, 0, 1), Font("Arial",  50));
		gl::disableAlphaBlending();
		return;
	}
	if (state == 1) {
		gl::enableAlphaBlending();
		gl::drawStringCentered("Type host IP: "+serverAddr, Vec2f(x, y), ColorA(0, 0, 0, 1), Font("Arial",  50));
		gl::disableAlphaBlending();
		return;
	}

	float xOff = -player.x*TILE_SIZE + x;
	float yOff = -player.y*TILE_SIZE + y;

	// draw chunks
	for (int i = 0; i < NUM_LOADED_CHUNKS; i++)
		chunks[i].draw(xOff, yOff);

	// draw particles
	for (int i = 0; i < NUM_PARTICLES; i++) {
		particle& p = particles[i];
		if (p.age <= 0) continue;
		gl::color(p.col);
		gl::drawSolidCircle(Vec2f(p.x*TILE_SIZE+xOff, p.y*TILE_SIZE+yOff), 5);
	}

	// draw player
	gl::color(Color(0, 1, 0));
	float rx = player.rx*TILE_SIZE, ry = player.ry*TILE_SIZE;
	gl::drawSolidRect(Rectf(x-rx, y-ry, x+rx, y+ry));

	// draw player2
	if (netio::started) {
		x = player2.x*TILE_SIZE+xOff;
		y = player2.y*TILE_SIZE+yOff;
		gl::drawSolidRect(Rectf(x-rx, y-ry, x+rx, y+ry));
	}

	// draw hunger bar
	gl::enableAlphaBlending();
	gl::drawString("Food:", Vec2f(45, 5), ColorA(0, 0, 0, 1), Font("Arial",  50));
	gl::disableAlphaBlending();
	gl::color(Color(1, 0, 0));
	gl::drawSolidRect(Rectf(50, 50, 50+300, 100));
	gl::color(Color(0, 1, 0));
	gl::drawSolidRect(Rectf(50, 50, 50+playerFood*3, 100));
}

bool BubbleFishApp::isBlockSolid(int x, int y) {
	int block = getBlock(x, y);
	switch (block) {
	case WALL:
		return true;
	default:
		return false;
	}
}

Chunk* BubbleFishApp::getChunk(int cx, int cy) {
	Chunk* c = NULL;
	for (int i = 0; i < NUM_LOADED_CHUNKS; i++) {
		if (chunks[i].cx==cx && chunks[i].cy==cy) {
			c = &chunks[i];
			break;
		}
	}
	return c;
}

int BubbleFishApp::getBlock(int x, int y) {
	int cx = x/CHUNK_SIZE;
	if (x < 0) cx--;
	int cy = y/CHUNK_SIZE;
	if (y < 0) cy--;

	Chunk* c = getChunk(cx, cy);

	if (!c) return EMPTY;
	while (x < 0) x += CHUNK_SIZE; // REALLY BAD!!!! TRY TO FIX
	while (y < 0) y += CHUNK_SIZE; // REALLY BAD!!!! TRY TO FIX
	int tx = x%CHUNK_SIZE;
	int ty = y%CHUNK_SIZE;
	return c->tiles[tx][ty];
}

void BubbleFishApp::setBlock(int x, int y, int block) {
	int cx = x/CHUNK_SIZE;
	if (x < 0) cx--;
	int cy = y/CHUNK_SIZE;
	if (y < 0) cy--;

	Chunk* c = getChunk(cx, cy);

	if (!c) return;
	while (x < 0) x += CHUNK_SIZE; // REALLY BAD!!!! TRY TO FIX
	while (y < 0) y += CHUNK_SIZE; // REALLY BAD!!!! TRY TO FIX
	int tx = x%CHUNK_SIZE;
	int ty = y%CHUNK_SIZE;
	c->tiles[tx][ty] = block;
}

CINDER_APP_NATIVE( BubbleFishApp, RendererGl )
