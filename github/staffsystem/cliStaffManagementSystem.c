//
// Created by 666 on 2021/12/28.
//
//


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>
#define M 20
#define N 256

// #include "cliStaffManagementSystem.h"

//#define tracepoint() printf("%s %s %d \n",__FILE__,__func__,__LINE__)

#define ERR_MSG(msg) do{\
	printf("line = %d\n",__LINE__);\
	perror(msg);\
}while(0)
/*
 * 员工管理系统功能需求
 * 员工信息：名字 手机号 地址 薪资 年龄等。包括但不限于上述信息
 * 区分管理员登录与普通员工登录
 * 管理员登录可以对员工信息进行增删改查。
 * 员工登录只能查看个人信息，可修改个人信息，但是不能修改薪资年龄等
 *
 * 技术需求：
 * 基于TCP
 * 多客户端并发
 */
typedef struct
{
	char type;//選擇選項 1、註冊2、登錄3、退出
	int state;
	char name[M];
	char gender;
	int age;
	char tel[13];
	char address[M];
	int level;//級別 1、管理官 0、普通員工
	float salary;
	char passwd[M];
}PACKAGE;
#define PACKLEN sizeof(PACKAGE)

typedef struct
{
	sqlite3 *db; //数据库
	int sfd;
	struct sockaddr_in cin;
	int newfd;
}cli_msg;



void __clear__()
{
	printf("* 按任意鍵清屏 *");
	while(getchar() != 10);
	system("clear");
}

// mainMenu
char mainMenu()
{
	printf("_ 3UM + J.LTD _\n");
	printf("| 1. Register |\n");
	printf("| 2. Login    |\n");
	printf("| 3. Exit     |\n");
	printf("--------------\n");
	printf(">> choose one >>\n");

	char choose;
	scanf("%c", &choose);
	while(getchar() != 10);

	return choose;
}

// admin_menu
char admin_menu()
{
	printf("--- A D M I N ---\n");
	printf("| 1. Search     |\n");
	printf("| 2. Alter      |\n");
	printf("| 3. Add        |\n");
	printf("| 4. Delete     |\n");
	printf("| 5. Back       |\n");
	printf("-----------------\n");
	printf(">> choose one >>\n");

	char choose;
	scanf("%c", &choose);
	while(getchar() != 10);

	return choose;
}

// general_menu
char general_menu()
{
	printf("---  general  ---\n");
	printf("| 1. Search     |\n");
	printf("| 2. Alter      |\n");
	printf("| 3. Back       |\n");
	printf("-----------------\n");
	printf(">> choose one >>\n");

	char choose;
	scanf("%c", &choose);
	while(getchar() != 10);

	return choose;
}
typedef void (*sighandler_t)(int);

int add_staff_msg(int sfd, PACKAGE* packmsg)
{
	printf(">> 0請輸入性別 M/F>> ");
	scanf("%c", &packmsg->gender);
	printf(">> 請輸入年齡 >> ");
	scanf("%d", &packmsg->age);
	printf(">> 請輸入聯繫電話 >> ");
	scanf("%s", packmsg->tel);
	printf(">> 請輸入住址 >> ");
	scanf("%s", packmsg->address);
	printf(">> 請輸入工資 >> ");
	scanf("%f", &packmsg->salary);
	printf(">> 請輸入密碼 >> ");
	scanf("%s", packmsg->passwd);
	if(send(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	return 0;

}




// 初始化 網絡配置
int __socket_init__(const char*IP,int PORT)
{
	//创建字节流式套接字
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0)
	{
		ERR_MSG("socket");
		return -1;
	}
	//允许端口复用
	int reuse = 1;
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		ERR_MSG("setsockopt");
		return -1;
	}

	// bind 绑定IP和端口PORT
	struct sockaddr_in sin;
	sin.sin_addr.s_addr = inet_addr(IP);
	sin.sin_port = htons(PORT);
	sin.sin_family = AF_INET;

	if(connect(sfd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
	{
		ERR_MSG("connect");
		return -1;
	}
	printf("connected OK >o<\n");

	return sfd;
}

// 登錄
int do_login(int sfd, PACKAGE* packmsg)
{
	// 數據包的type填充 
	packmsg->type = 'L';

	printf(">> your account name >> ");
	scanf("%s", packmsg->name);	
	while(getchar() != 10);
	printf(">> your account password >> ");
	scanf("%s", packmsg->passwd);	
	while(getchar() != 10);

	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	int flag;
	if(recv(sfd, &flag, sizeof(flag), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}
	printf("----%d---- %d\n", __LINE__, flag);

	if(flag == 0)
	{
		printf("未註冊， 請先註冊 \n");
		return -1;
	}
	else if(flag == 1)
	{
		printf("密碼錯誤 \n");
		return -1;
	}
	else if(flag == 2)
	{
		printf("重複登錄 \n");
		return -1;

	}
	else if(flag == 3)
	{
		printf("登錄成功 \n");
		// 若爲管理員 進入 admin_menu
		// 若爲普通員工 進入 general_menu
		int ret;
		if(recv(sfd, &ret, sizeof(ret), 0) < 0)
		{
			ERR_MSG("recv");
			return -1;
		}
		return ret;
	}
}


int add_msg(int sfd, PACKAGE* packmsg)
{
	printf(">> 0請輸入性別 M/F>> ");
	scanf("%c", &packmsg->gender);
	printf(">> 請輸入年齡 >> ");
	scanf("%d", &packmsg->age);
	printf(">> 請輸入聯繫電話 >> ");
	scanf("%s", packmsg->tel);
	printf(">> 請輸入住址 >> ");
	scanf("%s", packmsg->address);
	printf(">> 請輸入工資 >> ");
	scanf("%f", &packmsg->salary);

	if(send(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	return 0;
}

// 註冊
int do_register(int sfd, PACKAGE* packmsg)
{	
	char name[N] ="";
	char passwd[N] = "";
	// 數據包的type填充 
	packmsg->type = 'R';

	printf(">> your account name >> ");
	scanf("%s", name);	
	while(getchar() != 10);

	strcpy(packmsg->name, name);// 數據包存入名字信息

	// 將數據包傳給服務器，判斷是否註冊過
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);

	//接收服務器的判斷結果flag ，1已註冊，2未註冊過
	int flag;
	recv(sfd, &flag, sizeof(flag), 0);
	printf("%d,%d\n",__LINE__, flag);

	if(flag == 1) // 可以註冊
	{
		// 設置密碼
		printf(">> create a password >> ");
		scanf("%s", passwd);
		while(getchar() != 10);

		strcpy(packmsg->passwd, passwd);	// 將密碼設置到數據包信息中
		printf("mima is %s \n", packmsg->passwd);
		// 發送密碼給服務器
		if(send(sfd, packmsg, PACKLEN, 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}
		printf(" 註冊成功！\n");
		printf(" 請完善信息\n");

		add_msg(sfd, packmsg);

	}
	else if(flag == 0)
	{
		printf("!!! 重複註冊 請登錄 !!! \n");
	}
	return 0;
}

int gender_search_self(int sfd, PACKAGE* packmsg)
{
	// 數據包的type填充 
	packmsg->type = 'M';

	printf("---see Myself---\n");

	printf("%d,name is %s\n", __LINE__, packmsg->name);
#if 1
	// 發送 查看自己的信息
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);
#endif
	int row, column;
	recv(sfd, &column, 4, 0);
	recv(sfd, &row, 4, 0);
	printf("%d,%d\n", column, row);
	if(row == 0)
	{
		printf("NO ONE NAMED %s \n", packmsg->name);
	}
	//接收 查詢到的結果
	int i = 0;
	char presult[128] = "";
	for(i = 0; i< column * (row + 1); i++)
	{
		if(recv(sfd, presult, 128, 0) < 0)
		{
			ERR_MSG("send");
			return 0;
		}
		printf("%-10s", presult);

		if(i % column == 8)
		{
			printf("\n");
		}
	}
	printf("\n");


	return 0;
}

int admin_do_delete(int sfd, PACKAGE* packmsg)
{
	packmsg->type = 'D';
	printf("----admin delete---\n");

	char name[M] = "";
	printf(">> 請輸入要刪除的員工的姓名 >> ");
	scanf("%s", name);	
	while(getchar() != 10);

	strcpy(packmsg->name, name);// 數據包存入名字信息

	// 發送 判斷是否有此人 
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);

	int flag;
	if(recv(sfd, &flag, sizeof(flag), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}

	if(flag == 1)
	{
		printf("已刪除 \n");
	}
	else
	{
		printf("查無此人 \n");
	}
	return 0;
}


char general_modify_menu()
{
	printf("- 請選擇要更改的信息 -\n");
	printf("|    1. TEL          |\n");
	printf("|    2. ADDRESS      |\n");
	printf("----------------------\n");
	printf(">> 請選擇 >>   ");

	char choose;
	scanf("%c", &choose);
	while(getchar() != 10);

	return choose;
}

char modify_menu()
{
	printf("- 請選擇要更改的信息 -\n");
	printf("|    1. TEL          |\n");
	printf("|    2. ADDRESS      |\n");
	printf("|    3. SALARY       |\n");
	printf("----------------------\n");
	printf(">> 請選擇 >>   ");

	char choose;
	scanf("%c", &choose);
	while(getchar() != 10);
	return choose;
}

int admin_do_modify(int sfd, PACKAGE* packmsg)
{
	packmsg->type = 'A';
	printf("--- admin modify---\n");

	char name[M] = "";
	printf(">> 請輸入要修改信息的員工的姓名 >> ");
	scanf("%s", name);	
	while(getchar() != 10);

	strcpy(packmsg->name, name);// 數據包存入名字信息

	// 發送 判斷是否有此人
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);

	int row, column;
	recv(sfd, &column, 4, 0);
	recv(sfd, &row, 4, 0);
	printf("%d,%d\n", column, row);
	if(row == 0)
	{
		printf("NO ONE NAMED %s \n", packmsg->name);
		return 0;
	}
	else
	{
		packmsg->type = modify_menu();
		printf("%d,%c\n", __LINE__, packmsg->type);

		char tel[13];
		char add[20];
		float salary;
		switch(packmsg->type)
		{
		case '1':
			printf("請輸入修改後的電話： \n");
			scanf("%s", tel);
			strcpy(packmsg->tel, tel);
			break;
		case '2':
			printf("請輸入修改後的地址： \n");
			scanf("%s", add);
			strcpy(packmsg->address, add);
			break;
		case '3':
			printf("請輸入修改後的薪水： \n");
			scanf("%f", &salary);
			packmsg->salary = salary;
			break;

		default:;
		}
		if(send(sfd, packmsg, PACKLEN, 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}
		else
		{
			printf("× 修改成功 ×\n");
		}
	}

	// 發送 
	return 0;
}

int gender_alter_self(int sfd, PACKAGE* packmsg)
{
	// 數據包的type填充 
	packmsg->type = 'X';

	printf("--- modify myslef ---\n");

	// 將數據包傳給服務器, 修改自己
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);

	packmsg->type = general_modify_menu();
	printf("%d,%c\n", __LINE__, packmsg->type);

	char tel[13];
	char add[20];
	float salary;
	switch(packmsg->type)
	{
	case '1':
		printf("請輸入修改後的電話： \n");
		scanf("%s", tel);
		strcpy(packmsg->tel, tel);
		break;
	case '2':
		printf("請輸入修改後的地址： \n");
		scanf("%s", add);
		strcpy(packmsg->address, add);
		break;

	default:;
	}
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	else
	{
		printf("× 修改成功 ×\n");
	}


	return 0;
}

int admin_do_add(int sfd, PACKAGE* packmsg)
{
	// 數據包的type填充 
	packmsg->type = 'Z';

	printf("---admin add---\n");

	char name[M] = "";
	printf(">> 請輸入要增加的姓名 >> ");
	scanf("%s", name);	
	while(getchar() != 10);

	strcpy(packmsg->name, name);// 數據包存入名字信息
#if 1
	// 將數據包傳給服務器，判斷是否註冊過
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);
#endif
	int row;
	if(recv(sfd, &row, sizeof(row), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}

	if(row == 0)
	{
		PACKAGE msg;
		add_staff_msg(sfd, &msg);
#if 0
		if(send(sfd, &msg, PACKLEN, 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		
		}
#endif
	}
	printf("add successed\n");
	return 0;
}



int admin_do_search(int sfd, PACKAGE* packmsg)
{
	// 數據包的type填充 
	packmsg->type = 'S';

	printf("---admin search---\n");


	char name[M] = "";
	printf(">> 請輸入要查找的姓名 >> ");
	scanf("%s", name);	
	while(getchar() != 10);

	strcpy(packmsg->name, name);// 數據包存入名字信息

	// 將數據包傳給服務器，判斷是否註冊過
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("發送姓名：%s 成功\n", packmsg->name);

	int row, column;
	recv(sfd, &column, 4, 0);
	recv(sfd, &row, 4, 0);
	printf("%d,%d\n", column, row);
	if(row == 0)
	{
		printf("NO ONE NAMED %s \n", packmsg->name);
	}
	//接收 查詢到的結果
	int i = 0;
	char presult[128] = "";
	for(i = 0; i< column * (row + 1); i++)
	{
		if(recv(sfd, presult, 128, 0) < 0)
		{
			ERR_MSG("send");
			return 0;
		}
		printf("%-10s", presult);

		if(i % column == 8)
		{
			printf("\n");
		}
	}
	printf("\n");


	return 0;

}


int do_quit(int sfd, PACKAGE* packmsg)
{
	packmsg->type = 'Q';
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("----quit ---\n");
	exit(0);
	return 0;
}


int do_back(int sfd, PACKAGE* packmsg)
{
	packmsg->type = 'B';
	if(send(sfd, packmsg, PACKLEN, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	printf("---- welcome ---\n");

	return 0;
}



// mainloop  
int mainloop(int sfd)
{
	char choose;

	PACKAGE msg;
	while(1)
	{
		choose = mainMenu();
		printf("no.%d:choose:%c\n", __LINE__, choose);

		switch(choose)
		{
		case '1':
			printf("Register\n");
			do_register(sfd, &msg);
			break;
		case '2':
			printf("login\n");
			int isAdmin;
			isAdmin = do_login(sfd, &msg); // 返回值判斷是否爲管理員


			if(isAdmin == 1)
			{
				int num;
				while(1)
				{
					num = admin_menu();
					switch(num)
					{
					case '1': 
						printf("%d\n", __LINE__);
						printf("Search\n");
						admin_do_search(sfd, &msg);
						break;
					case '2':
						printf("admin Alter\n");
						admin_do_modify(sfd, &msg);
						break;
					case '3':
						printf("admin add staff\n");
						admin_do_add(sfd, &msg);
						break;
					case '4':
						printf("admin delete staff\n");
						admin_do_delete(sfd, &msg);
						break;
					case '5':
						printf("admin back to mainmenu\n");
						do_back(sfd, &msg);
						break;

					default:;
					}
					//		__clear__();
					if(num == '5')
					{
						break;
					}

				}
			}
			else if(isAdmin == 0)
			{
				int num;
				while(1)
				{
					num = general_menu();
					switch(num)
					{	case '1': 
						printf("%d\n", __LINE__);
						printf("See Myself\n");
						gender_search_self(sfd, &msg);
						break;
					case '2':
						printf("general Alter\n");
						gender_alter_self(sfd, &msg);
						break;
					case '3':
						printf("general Back\n");
						do_back(sfd, &msg);
						break;
					default:;
					}
					//		__clear__();
					if(num == '3')
					{
						break;
					}
				}
			}
			break;
		case '3':
			printf("exit\n");
			do_quit(sfd, &msg);
			break;
		default:
			printf("請重新輸入選擇>> \n");
			continue;
		}
		__clear__();
	}

	return 0;
}

int main(int argc, const char *argv[])
{
	// 信号处理函数 ctrl+c
	/*
	   sighandler_t sig = signal(SIGINT,handler);
	   if (sig == SIG_ERR)
	   {
	   ERR_MSG("signal");
	   return -1;
	   }
	   */
	//外部傳參 IP:argv[1], PORT:argv[2]
	if (argc < 3) {
		fprintf(stderr, "please enter IP & PORT number : \n");
		return -1;
	}
	// socket
	int sfd = __socket_init__(argv[1],atoi(argv[2]));

	if(sfd == -1)
	{
		return -1;
	}
	else
	{
		printf("mainloop [%d]\n", __LINE__);
		mainloop(sfd);
	}
	return 0;
}
