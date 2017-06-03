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

GLubyte frame[WIDTH][HEIGHT][3];//�洢�����RGBֵ
double line_z[WIDTH+1];//�洢һ�е����

struct Color{
	unsigned char r, g, b;
};

struct Edge{
	double hx,hy,lx,ly,dx;
	int dy;
	struct Edge* next;
};

//����νṹ��
typedef struct Poly{
	double a, b, c, d;
	int id,dy;
	double ly;
	struct Color color;
	struct Poly* next;
	struct Edge* edge;
}Poly;
Poly polygons[MAX_FACE];

//����α�
Poly* polylist[HEIGHT];

//��߽ڵ�
struct AENode{
	double xl, dxl, dy, xr, dxr, zl, dzx, dzy;
	int id;
	struct AENode* next;
};
struct AENode* AEhead; //��߱�

//��ȡobj�ļ�������vertexs��faces
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

//��vertexs������������ź�ƽ�ƣ�ʹ����ͼ���ܹ���Ӧ����
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

//��������p��ƽ��ϵ��
inline void processCoeffi(struct Poly*p, double *v1, double* v2,double* v3 )
{
	double x1 = v1[0], x2 = v2[0], x3 = v3[0], y1 = v1[1], y2 = v2[1], y3 = v3[1],
		z1 = v1[2], z2 = v2[2], z3 = v3[2];
	p->a = (y2 - y1)*(z3 - z1) - (y3 - y1)*(z2 - z1);
	p->b = (z2 - z1)*(x3 - x1) - (z3 - z1)*(x2 - x1);
	p->c = (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2 - y1);
	p->d = -p->a*x1 - p->b*y1 - p->c*z1;
}

//���ݷ�������z��ĽǶ���������������
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

//��vertexs��faces�е����ݹ���Poly��Edge�ṹ��
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

//��ɨ����y������p�ཻ�ı߶ԣ�����ae
int findEdgePair(int y, struct Poly*p, AENode* ae)
{
	struct Edge* el = NULL, *er = NULL;
	double xl = INT_MAX, xr = INT_MIN;

	//��������εı�
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

	//û�ҵ�һ��
	if (el == NULL || er == NULL || el == er)
		return 0;

	//����AENode
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

//ɨ����Z�����㷨
void lineZbuffer(){
	for (int i = HEIGHT - 1; i >= 0; i--)
	{
		//add new polygon from polylist to ��߱�(AE)
		for (struct Poly* p = polylist[i]; p != NULL;)
		{
			if (p->dy > 0){
				AENode* ae = (AENode*)malloc(sizeof(struct AENode));
				if (!findEdgePair(i, p, ae)){  
					//����������ߵ�պ���i��ʱ����Ҳ���һ�Աߣ���Ϊɨ���������ߵĽ����غ�
					delete ae;
					if (i > 0){
						//�����Ƶ�polylist����һ����
						p->dy--;
						struct Poly* temp = p->next;
						p->next = polylist[i - 1];
						polylist[i - 1] = p;
						p = temp;
						continue;
					}
				}
				else{
					//�����߱�
					ae->next = AEhead;
					AEhead = ae;
				}
			}
			p = p->next;
		}

		//�����߱����Ѿ�����ı߶ԣ�AENode��
		for (struct AENode** p = &AEhead; (*p) != NULL;)
		{
			if ((*p)->dy <= 0)
			{
				int id = (*p)->id;
				int ly = polygons[id].ly;
				if (i <= ly || !findEdgePair(i, &polygons[id], (*p)) ){ //���������Ѿ����� ���� �Ҳ���һ�Աߣ�ʵ��δ���ָ�������֣�
					//ɾ���߶�
					struct AENode* temp = (*p);
					(*p) = (*p)->next;
					delete temp;
					continue;
				}
			}
			p = &(*p)->next;
		}

		//��ʼ�����
		for (int j = 0; j < WIDTH; j++)
			line_z[j] = INT_MIN;

		//ɨ���߱��еĸ��߶�
		for (struct AENode* p = AEhead; p != NULL; p = p->next)
		{
			double xl = p->xl, xr = p->xr, zl = p->zl;
			int x = (int)xl + 1; //�ҳ�xl�ұߵ�һ�����ص�
			if (x < 0) x = 0;
			if (x >= WIDTH) continue;
			if (xr > WIDTH) xr = WIDTH;
			double z = zl + (x - xl)*p->dzx;

			//ɨ������
			while (x < xr)
			{
				if (z > line_z[x]){
					line_z[x] = z;
					setColor(x,i, p->id);
				}
				z += p->dzx;
				x++;
			}

			//������Ϣ��Ϊ��һ����׼��
			p->dy--;
			p->zl += p->dzy;
			p->xl += p->dxl;
			p->xr += p->dxr;
		}
	}
}

//OpenGL���ƺ���
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
	//OpenGL��ʼ��
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

	//��ȡobj�ļ�������vertexs��faces
	inputObj("Object/bunny.obj",MODE::GRAY); //ע�⣡����ֻ�ж��������С�ڵ���3��ʱ�������MODE::RGB����ģʽ���ڲ�����ȷ�ԡ���������MODE::GRAY��
	printf("%d faces, %d vertexs\n", size_face, size_vertex);
	//��vertexs������������ź�ƽ�ƣ�ʹ����ͼ���ܹ���Ӧ����
	adjustData();
	//��vertexs��faces�е����ݹ���Poly��Edge�ṹ��
	processData();
	//ɨ����Z�����㷨
	clock_t time_cal_start = clock();
	lineZbuffer();
	printf("algorithm costs %lf s.\n", double(clock() - time_cal_start) / CLOCKS_PER_SEC);

	printf("total costs %lf s.\n", double(clock() - time_all_start) / CLOCKS_PER_SEC);

	//OpenGL���
	glutDisplayFunc(display);
	//glutIdleFunc(NULL);
	glutMainLoop();
}