#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <math.h>
#include <time.h>

#define DEBUGGING 0
#define MIN(a,b) ((a)<(b)?(a):(b));
enum MODE{GRAY,RGB} mode;

const int MAX_VERTEX = 100000;
const int MAX_FACE = 100000;
const int MAX_VERTEX_PER_FACE = 20;

const int WIDTH = 1200;
const int HEIGHT = 800;

int size_vertex = 0, size_face = 0;
double vertexs[MAX_VERTEX][3];
int faces[MAX_FACE][MAX_VERTEX_PER_FACE+1];

GLubyte frame[WIDTH][HEIGHT][3];//存储结果的RGB值
double line_z[WIDTH+1];//存储一行的深度

struct Color{
	unsigned char r, g, b;
};

struct Edge{
	double hx,hy,lx,ly,dx;
	int dy;
	struct Edge* next;
};

//多边形结构体
typedef struct Poly{
	double a, b, c, d;
	int id,dy;
	double ly;
	struct Color color;
	struct Poly* next;
	struct Edge* edge;
}Poly;
Poly polygons[MAX_FACE];

//多边形表
Poly* polylist[HEIGHT];

//活化边节点
struct AENode{
	double xl, dxl, dy, xr, dxr, zl, dzx, dzy;
	int id;
	struct AENode* next;
};
struct AENode* AEhead; //活化边表

//读取obj文件，存入vertexs和faces
void inputObj(const char *file, int mode)
{
	FILE *f = fopen(file, "r");
	if (!f)
	{
		printf("can't find the obj file");
		return;
	}
	const int MAX_LINE_CHAR = 200;
	char line[MAX_LINE_CHAR];
	char type[MAX_LINE_CHAR];
	double x, y, z;

	while (fgets(line, MAX_LINE_CHAR, f))
	{
		if (sscanf(line, "%s", type)!=1) continue;
		if (!strcmp(type, "v")){
			if (sscanf(line + 1, "%lf%lf%lf", &vertexs[size_vertex][0], 
				 &vertexs[size_vertex][1], &vertexs[size_vertex][2]) != 3)
			{
				printf("[ERROR] when sscanf vertex:%s", line);
				return;
			}
			if (++size_vertex >= MAX_VERTEX){
				printf("[ERROR] size_vertex>MAX_VERTEX");
				return;
			}
		}
		else if (!strcmp(type, "f"))
		{
			char* temp = line+1;
			int num = 0;
			while (temp = strchr(temp, ' '))
			{
				if (sscanf(temp, "%d", &faces[size_face][num+1]) != 1)
					break;
				num++;
				temp += strspn(temp, " ");
			}
			if (num < 3)
			{
				printf("[ERROR] face vertexs less than 3! :%s", line);
				return;
			}
			faces[size_face][0] = num;
			if (++size_face >= MAX_FACE)
			{
				printf("[ERROR] size_face>MAX_FACE");
				return;
			}
		}
	}
}

//对vertexs的坐标进行缩放和平移，使最终图像能够适应窗口
void adjustData()
{
	double xmax=INT_MIN, xmin=INT_MAX, ymax=INT_MIN, ymin = INT_MAX;
	for (int i = 0; i < size_vertex; i++)
	{
		if (vertexs[i][0] > xmax) xmax = vertexs[i][0];
		if (vertexs[i][0] < xmin) xmin = vertexs[i][0];
		if (vertexs[i][1] > ymax) ymax = vertexs[i][1];
		if (vertexs[i][1] < ymin) ymin = vertexs[i][1];
	}
	int offsetx = WIDTH/2 - (xmax + xmin) / 2;
	int offsety = HEIGHT/2 - (ymax + ymin) / 2;

	//double scale = (WIDTH / (xmax - xmin) + HEIGHT / (ymax - ymin)) / 2;
	double scale =  MIN(WIDTH / (xmax - xmin), HEIGHT / (ymax - ymin));

	if (scale>0.5 && scale < 2) scale=1;

	for (int i = 0; i < size_vertex; i++)
	{
		vertexs[i][0] = vertexs[i][0] * scale + offsetx;
		vertexs[i][1] = vertexs[i][1] * scale + offsety;
		vertexs[i][2] = vertexs[i][2] * scale;
	}
}

//计算多边形p的平面系数
inline void processCoeffi(struct Poly*p, double *v1, double* v2,double* v3 )
{
	double x1 = v1[0], x2 = v2[0], x3 = v3[0], y1 = v1[1], y2 = v2[1], y3 = v3[1],
		z1 = v1[2], z2 = v2[2], z3 = v3[2];
	p->a = (y2 - y1)*(z3 - z1) - (y3 - y1)*(z2 - z1);
	p->b = (z2 - z1)*(x3 - x1) - (z3 - z1)*(x2 - x1);
	p->c = (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2 - y1);
	p->d = -p->a*x1 - p->b*y1 - p->c*z1;
}

//根据法向量与z轴的角度来给予多边形亮度
void caculateColor(Poly* p)
{
	if (mode == MODE::GRAY){
		double l = sqrt(pow(p->a, 2) + pow(p->b, 2) + pow(p->c, 2));
		double cos = p->c / l;
		double temp = abs(cos * 255);
		p->color.r = p->color.g = p->color.b = temp;
	}
	else{
		switch (p->id){
		case 0:
			p->color.r = 150;
			break;
		case 1:
			p->color.g = 150;
			break;
		case 2:
			p->color.b = 150;
			break;
		}
	}
}

//用vertexs、faces中的数据构建Poly和Edge结构体
void processData()
{
	int h, l, t;
	for (int i = 0; i < size_face; i++)
	{
		int maxy = INT_MIN, miny = INT_MAX;
		for (int j = 0; j < faces[i][0]; j++)
		{
			h = faces[i][j+1]-1;
			if (j + 1 == faces[i][0])
				l = faces[i][1]-1;
			else
				l = faces[i][j+2]-1;
			if (vertexs[h][1] < vertexs[l][1])
			{
				t = h;
				h = l;
				l = t;
			}
			struct Edge* e = (struct Edge*)malloc(sizeof(struct Edge));
			e->hx = vertexs[h][0];
			e->hy = vertexs[h][1];
			e->lx = vertexs[l][0];
			e->ly = vertexs[l][1];
			e->dx = -(vertexs[h][0] - vertexs[l][0])/(vertexs[h][1] - vertexs[l][1]);
			e->dy = int(vertexs[h][1] + 1e-6) - int(vertexs[l][1] + 1e-6);
			e->next = polygons[i].edge;
			polygons[i].edge = e;

			if (int(vertexs[h][1] + 1e-6) > maxy) maxy = int(vertexs[h][1] + 1e-6);
			if (int(vertexs[l][1] + 1e-6) < miny) miny = int(vertexs[l][1] + 1e-6);
		}
		if (maxy >= HEIGHT) maxy = HEIGHT - 1;
		polygons[i].dy = maxy - miny;
		if (polygons[i].dy <= 0) continue;
		polygons[i].next = polylist[maxy];
		polylist[maxy] = &polygons[i];
		polygons[i].ly = miny;
		polygons[i].id = i;

		processCoeffi(&polygons[i], vertexs[faces[i][1]-1], vertexs[faces[i][2]-1], vertexs[faces[i][3]-1]);
		caculateColor(&polygons[i]);
	}
}

//找扫描线y与多边形p相交的边对，存入ae
int findEdgePair(int y, struct Poly*p, AENode* ae)
{
	struct Edge* el = NULL, *er = NULL;
	double xl = INT_MAX, xr = INT_MIN;

	//遍历多边形的边
	for (struct Edge* i = p->edge; i != NULL; i = i->next)
		if (i->dy > 0 && i->hy + 1e-6 >= y && i->ly + 1e-6 < y)
		{
			double x = (i->hx*y - i->hx*i->ly - i->lx*y + i->lx*i->hy) / (i->hy - i->ly);
			if (x < xl){
				xl = x;
				el = i;
			}
			if (x > xr)
			{
				xr = x;
				er = i;
			}
		}

	//没找到一对
	if (el == NULL || er == NULL || el == er)
		return 0;

	//设置AENode
	ae->xl = xl;
	ae->xr = xr;
	ae->id = p->id;
	ae->dxl = el->dx;
	ae->dxr = er->dx;
	int dyl = y - int(el->ly + 1e-6);
	int dyr = y - int(er->ly + 1e-6);
	ae->dy = dyl < dyr ? dyl : dyr;
	if (ae->dy == 0) printf("[ERROR] in findEdgePair. ae->dy==0");
	if (abs(p->c)>1e-15){
		ae->zl = -(p->d + p->a*xl + p->b*y) / p->c;
		ae->dzx = -p->a / p->c;
		ae->dzy = p->b / p->c +ae->dzx * ae->dxl;
	}
	else{
		ae->zl = INT_MIN;
		ae->dzx = 0;
		ae->dzy = 0;
	}

	return 1;
}

inline void setColor(int x, int y, int id)
{
	*(frame[0][0] + (WIDTH*y + x) * 3) = polygons[id].color.r;
	*(frame[0][0] + (WIDTH*y + x) * 3+1) = polygons[id].color.g;
	*(frame[0][0] + (WIDTH*y + x) * 3+2) = polygons[id].color.b;
}

//扫描线Z缓冲算法
void lineZbuffer(){
	for (int i = HEIGHT - 1; i >= 0; i--)
	{
		//add new polygon from polylist to 活化边表(AE)
		for (struct Poly* p = polylist[i]; p != NULL;)
		{
			if (p->dy > 0){
				AENode* ae = (AENode*)malloc(sizeof(struct AENode));
				if (!findEdgePair(i, p, ae)){  
					//比如多边形最高点刚好是i的时候会找不到一对边，因为扫描线与两边的交点重合
					delete ae;
					if (i > 0){
						//将它移到polylist的下一级中
						p->dy--;
						struct Poly* temp = p->next;
						p->next = polylist[i - 1];
						polylist[i - 1] = p;
						p = temp;
						continue;
					}
				}
				else{
					//加入活化边表
					ae->next = AEhead;
					AEhead = ae;
				}
			}
			p = p->next;
		}

		//处理活化边表中已经用完的边对（AENode）
		for (struct AENode** p = &AEhead; (*p) != NULL;)
		{
			if ((*p)->dy <= 0)
			{
				int id = (*p)->id;
				int ly = polygons[id].ly;
				if (i <= ly || !findEdgePair(i, &polygons[id], (*p)) ){ //如果多边形已经用完 或者 找不到一对边（实际未发现该情况出现）
					//删除边对
					struct AENode* temp = (*p);
					(*p) = (*p)->next;
					delete temp;
					continue;
				}
			}
			p = &(*p)->next;
		}

		//初始化深度
		for (int j = 0; j < WIDTH; j++)
			line_z[j] = INT_MIN;

		//扫描活化边表中的各边对
		for (struct AENode* p = AEhead; p != NULL; p = p->next)
		{
			double xl = p->xl, xr = p->xr, zl = p->zl;
			int x = (int)xl + 1; //找出xl右边第一个像素点
			if (x < 0) x = 0;
			if (x >= WIDTH) continue;
			if (xr > WIDTH) xr = WIDTH;
			double z = zl + (x - xl)*p->dzx;

			//扫描区间
			while (x < xr)
			{
				if (z > line_z[x]){
					line_z[x] = z;
					setColor(x,i, p->id);
				}
				z += p->dzx;
				x++;
			}

			//更新信息，为下一次做准备
			p->dy--;
			p->zl += p->dzy;
			p->xl += p->dxl;
			p->xr += p->dxr;
		}
	}
}

//OpenGL绘制函数
void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glRasterPos2i(0, 0);
	glDrawPixels(WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, frame);
	glutSwapBuffers();
}

void main(int argc, char** argv)
{
	//OpenGL初始化
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("LineZBuffer");
	glClearColor(0.75f, 0.75f, 0.75f, 1.0f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, WIDTH << 6, 0, HEIGHT << 6);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	clock_t time_all_start = clock();

	//读取obj文件，存入vertexs和faces
	inputObj("Object/bunny.obj",MODE::GRAY); //注意！！！只有多边形数量小于等于3的时候可以用MODE::RGB，该模式用于测试正确性。否则请用MODE::GRAY。
	printf("%d faces, %d vertexs\n", size_face, size_vertex);
	//对vertexs的坐标进行缩放和平移，使最终图像能够适应窗口
	adjustData();
	//用vertexs、faces中的数据构建Poly和Edge结构体
	processData();
	//扫描线Z缓冲算法
	clock_t time_cal_start = clock();
	lineZbuffer();
	printf("algorithm costs %lf s.\n", double(clock() - time_cal_start) / CLOCKS_PER_SEC);

	printf("total costs %lf s.\n", double(clock() - time_all_start) / CLOCKS_PER_SEC);

	//OpenGL相关
	glutDisplayFunc(display);
	//glutIdleFunc(NULL);
	glutMainLoop();
}