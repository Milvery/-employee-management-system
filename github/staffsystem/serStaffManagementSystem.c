//
// Created by 666 on 2021/12/28.
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
#define M 20
#define N 256

#define trace() printf("%s,%s,%d \n",__FILE__,__func__,__LINE__)
//#include "serStaffManagementSystem.h"

#define ERR_MSG(msg) do{ \
   fprintf(stderr, "%d,%s ", __LINE__, __func__); \
   perror(msg);  \
}while(0)

typedef struct
{
    sqlite3 *db; //数据库
    struct sockaddr_in cin;
    int newfd;
}cli_msg;

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

int judge(cli_msg* info, char *sql, int* row, int* column, char*** presult)
{
	char *errmsg;
	if(sqlite3_get_table(info->db, sql, presult, row, column, &errmsg) != SQLITE_OK)
	{
		// 錯誤
		printf("[%d]:sqlite3_get_table:%s\n", __LINE__, errmsg);
	}
	return 0;	
}

// 退出
int do_quit(cli_msg* info, PACKAGE* packmsg)
{
	sqlite3* db = info->db;
	int sfd = info->newfd;
	struct sockaddr_in cin = info->cin;

	printf("----tuichu ----\n");
	char sql[N] = "";
	char* errmsg = NULL;
	sprintf(sql, "update staffSheet set STATE=0 where NAME=\"%s\";", packmsg->name);// package.name
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("[%d]:sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;
	}
	printf("[%s,%d] name=%s quit\n",inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), packmsg->name);

	return 0;
}











int is_admin(cli_msg* info, PACKAGE* packmsg)
{
	char sql[N] = "";
	int row, column;
	char * errmsg = NULL;
	char** presult = NULL;

	sprintf(sql, "select LEVEL from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column, &errmsg) != SQLITE_OK)
	{
		// 錯誤
		printf("[%d]:sqlite3_get_table:%s", __LINE__, errmsg);
		return -1;
	}
	printf("[%d]: 員工權限%s \n", __LINE__, presult[1]);


	return presult[1][0] == '1' ? 1: 0;
}


// 网络初始化
int __socket_init__(const char*IP, int PORT)
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

    if(bind(sfd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        ERR_MSG("bind");
        return -1;
    }
    printf("bind OK >o<\n");

    // listen 设置被动监听状态
    if (listen(sfd, 6) < 0)
    {
        ERR_MSG("listen");
        return -1;
    }
    printf("listen OK >o<\n");
    printf("網絡初始化成功 正在監聽鏈接中 >o< ...\n");

    return sfd;
}


int create_staffmsg(sqlite3* db)//員工信息表格
{
	// 員工表格的建立  staffSheet
	char sql[N];
	sprintf(sql, "Create table if not exists staffSheet(NAME char primary key, GENDER char,\
				   AGE int, TEL char, ADDRESS char, SALARY float, \
				   PASSWD char, STATE int, LEVEL int);");
	char* errmsg = NULL;
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg)!= 0)
	{
		printf("[%d]:sqlite3_exec:%s\n", __LINE__, errmsg);
		return -1;;
	}
	printf("staffmsg form create successed\n");
 
	return 0;
}


// 员工staffDb数据库初始化
int __staffDb_init__(sqlite3* db)
{
	FILE* fp_r = NULL; 

	// 管理員表格的建立
	char buf[N] = "create table admin(NAME char);";

	char* errmsg = NULL;
	int res = sqlite3_exec(db, buf, NULL, NULL, &errmsg);
	if(res != 0)
	{
		if(res == 1)
		{
			printf("管理員表格存在\n");
			printf("是否刪除原有管理員數據 重新載入?(y/n)\n");
			
			char choose;
			scanf("%c", &choose);
			if(choose != 'y')// 不重新載入
			{
				printf("繼續使用原有管理員模式\n");
				return 1;
			}
			else // 刪除管理員表格 重新載入
			{
			//	char drop[N] = "drop table admin;";
				if(sqlite3_exec(db, "drop table admin;", NULL, NULL, &errmsg) != SQLITE_OK)
				{
					printf("[%d],%s\n", __LINE__, errmsg);
					return -1;
				}
			}

			if(sqlite3_exec(db, buf, NULL, NULL, &errmsg) != SQLITE_OK)
			{
				printf("創建管理員表格失敗\n");
				printf("[%d]: %s\n", __LINE__, errmsg);
				return -1;
			}

		//	// 員工表格的建立 
			printf("------------\n");
		}
	}

	create_staffmsg(db);//員工信息表格

	// 載入管理員信息
	fp_r = fopen("./administrator.txt","r");
	if(fp_r == NULL)
	{
		perror("fopen");
		return -1;
	}
	printf("打開管理員文件成功\n");
	printf("載入中...\n");

	char name[N] = "";

	char tmp[N] = "";
	char ch[N] = "";
	while(fgets(ch, sizeof(ch),fp_r) != NULL)
	{
		sscanf(ch, "%[^\n]", name);
		sprintf(tmp, "insert into admin values(\"%s\");", name);
		if(sqlite3_exec(db, tmp, NULL, NULL, &errmsg) != 0)
		{
			printf("[%d]:sqlite3_exec:%s\n", __LINE__, errmsg);
			return -1;
		}
		bzero(ch, sizeof(ch));
	}
//	fclose(fp_r);
	return 0;
}

// 登錄
int do_login(PACKAGE* packmsg, cli_msg* info)
{
	// 判斷是否有註冊  NAME
	// 是否有重複登錄 STATES

	// 接收客戶端數據包信息(NAME)
	trace();
    struct sockaddr_in cin = info->cin;
	int sfd = info->newfd;
	printf("[%s : %d] newfd = %d 正在嘗試登錄\n",inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), info->newfd);
#if 0
	if(recv(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}
	printf("收到含姓名密碼數據包\n");
#endif

	//判斷是否有註冊
	char sql[N] = "";
	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);

	int row, column;
	char * errmsg = NULL;
	char** presult = NULL;
	judge(info, sql, &row, &column, &presult);	

	int flag = 0;

	if(row == 0) // 未註冊過
	{
		printf("[%s]:未註冊\n",packmsg->name);
		// 發送註冊狀態 0---未註冊過
		flag = 0;
		if(send(sfd, &flag, sizeof(flag), 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}
	}
	else
	{
		printf("[%s]:已註冊 \n", packmsg->name);

		// 判斷密碼是否正確		

		printf("-----%d,%s,%s,%s,%s, %s\n", __LINE__, presult[12], presult[13], presult[14], presult[15], presult[16]);
		if(strcmp(packmsg->passwd, presult[15]) == 0)
		{
			// 判斷是否重複登錄
			if(presult[16][0] == '1')// 重複登錄
			{
				flag = 2;
				if(send(sfd, &flag, sizeof(flag), 0) < 0)
				{
					ERR_MSG("send");
					return -1;
				}
			}
			else
			{
				//登錄成功 並修改STATE狀態
				flag = 3;
				sprintf(sql, "update staffSheet set STATE=1 where NAME=\"%s\";", packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}
				if(send(sfd, &flag, sizeof(flag), 0) < 0)
				{
					ERR_MSG("send");
					return -1;
				}
				// 判斷是否爲管理員 進入不同界面
				int ret;
				ret = is_admin(info, packmsg);
				if(send(sfd, &ret, sizeof(ret), 0) < 0)
				{
					ERR_MSG("send");
					return -1;
				}
			}
		}
		else
		{
			// 密碼錯 flag = 1
			flag = 1;
			if(send(sfd, &flag, sizeof(flag), 0) < 0)
			{
				ERR_MSG("send");
				return -1;
			}
		}
	}

	printf("----%d------%d\n", __LINE__, flag);
#if 0
	if(send(sfd, &flag, sizeof(flag), 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
#endif
	printf("----%d------%d\n", __LINE__, flag);



	sqlite3_free_table(presult);

	return 0;
}













// 註冊
int do_register(PACKAGE* packmsg, cli_msg* info)
{
	// 接收客戶端數據包信息
	//（判斷是否註冊過 姓名判斷）
	// 和數據庫數據表作對比
	// 若爲新註冊 flag=1，添加到信息表
	// 已註冊 則提示“重複”，並返回mainmenu

    struct sockaddr_in cin = info->cin;
	sqlite3* db = info->db;
	int sfd = info->newfd;
	printf("[%s : %d] newfd = %d 正在註冊\n",inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), info->newfd);
#if 0
	if(recv(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}
	printf("收到含姓名數據包\n");
#endif
	char sql[N] = "";
	char* errmsg = NULL;
	int flag = 0;
	trace();
	bzero(sql, sizeof(sql));
	sprintf(sql, "insert into staffSheet(NAME) values(\"%s\");", packmsg->name);
	printf("%d:%s\n", __LINE__, packmsg->name);

	printf("%p\n", db);
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		trace();
		send(info->newfd, &flag, sizeof(flag), 0);
		printf("[StaffName : %s] 已註冊\n",packmsg->name);
		return -1;
	}
	else
	{
		trace();
		// 未註冊過, 發送客戶端判斷結果
		flag = 1;
		if(send(sfd, &flag, sizeof(flag), 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}

		// 接收密碼
		recv(sfd, packmsg, sizeof(PACKAGE), 0);
		trace();
		printf("密碼是 %s \n", packmsg->passwd);
		// 添加進表格 
		bzero(sql, sizeof(sql));
		sprintf(sql, "update staffSheet set PASSWD=\"%s\" where NAME=\"%s\";", packmsg->passwd, packmsg->name);
		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("[%d]:sqlite3_exec:%s\n", __LINE__, errmsg);
			return -1;
		}

		packmsg->state = 0; // 信息表STATE即登錄狀態=0
		bzero(sql, sizeof(sql));
		sprintf(sql, "update staffSheet set STATE=%d where NAME=\"%s\";", packmsg->state, packmsg->name);
		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("[%d]:sqlite3_exec:%s\n", __LINE__, errmsg);
			return -1;
		}
		// 添加成功
		printf("[%s:%d][staffname:%s] 註冊成功 >o< \n",inet_ntoa(info->cin.sin_addr), ntohs(info->cin.sin_port), packmsg->name);
		
				if(recv(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
				{
					ERR_MSG("recv");
					return -1;
				}
				printf("%s,%d\n", packmsg->address, __LINE__);
				// 將信息包內容載入數據庫
				bzero(sql, sizeof(sql));
				sprintf(sql, "update staffSheet set GENDER=\"%c\" where NAME=\"%s\";", packmsg->gender, packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}
				
				bzero(sql, sizeof(sql));
				sprintf(sql, "update staffSheet set AGE=%d where NAME=\"%s\";", packmsg->age, packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}

				bzero(sql, sizeof(sql));
				sprintf(sql, "update staffSheet set TEL=\"%s\" where NAME=\"%s\";", packmsg->tel, packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}

				bzero(sql, sizeof(sql));
				sprintf(sql, "update staffSheet set ADDRESS=\"%s\" where NAME=\"%s\";", packmsg->address, packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}

				bzero(sql, sizeof(sql));
				sprintf(sql, "update staffSheet set SALARY=\"%.2f\" where NAME=\"%s\";", packmsg->salary, packmsg->name);
				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}

				// 判斷是否爲管理員並更新LEVEL 狀態
				int row, column;
				char * errmsg = NULL;
				char** presult = NULL;
				char sql[N] = "";
				sprintf(sql, "select * from admin where NAME=\"%s\";", packmsg->name);
				if(sqlite3_get_table(info->db, sql, &presult, &row, &column, &errmsg) != SQLITE_OK)
				{
					// 錯誤
					printf("[%d]:sqlite3_get_table:%s", __LINE__, errmsg);
					return -1;
				}


				if(row == 1)
				{
					// 是管理員
					bzero(sql, sizeof(sql));
					sprintf(sql, "update staffSheet set LEVEL=1 where NAME=\"%s\";", packmsg->name);
					trace();

				}
				else
				{
					trace();
					bzero(sql, sizeof(sql));
					sprintf(sql, "update staffSheet set LEVEL=0 where NAME=\"%s\";", packmsg->name);
				}

				if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
				{
					trace();
					return -1;
				}

	}
	trace();
	return 0;
}

int admin_add(PACKAGE* packmsg, cli_msg* info)
{
	// 接收 查找此人
	printf("[%d] admin add [%s]\n", __LINE__, packmsg->name);

	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column,&errmsg) != 0)
	{
		printf("[%d]:sqlite3_get_table:%s\n",__LINE__,errmsg);
	}

	printf("%d,%d\n", column, row);
	if(row != 0)
	{
		printf("已有[%s] 請勿重複增加 \n", packmsg->name);
	}
	else
	{
		// 無 可以添加此人
		if(send(sfd, &row, sizeof(row), 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}
		trace();
		PACKAGE msg;
		if(recv(sfd, &msg, sizeof(msg), 0) < 0)
		{
			ERR_MSG("recv");
			return -1;
		}
		// 接收成功  加入數據庫中
		sprintf(sql, "insert into staffSheet values(\"%s\", \"%c\", \"%d\", \"%s\", \"%s\", \"%f\", \"%s\", \"%d\", \"%d\"); ", \
				packmsg->name, msg.gender, msg.age, msg.tel, msg.address, msg.salary, msg.passwd, 0, 0);
		printf("%s\n", sql);
		if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
			return -1;
		}
		printf("add[%s] successed\n", packmsg->name);


	}


	return 0;
}

int general_search(PACKAGE* packmsg, cli_msg* info)
{
	// 接收 查找此人信息
	printf("[%d] general search [%s]\n", __LINE__, packmsg->name);

	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column,&errmsg) != 0)
	{
		printf("[%d]:sqlite3_get_table:%s\n",__LINE__,errmsg);
	}

	printf("%d,%d\n", column, row);
	if(row == 0)
	{
		printf("no one named %s\n", packmsg->name);
	}

	if(send(sfd, &column, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	if(send(sfd, &row, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	int i = 0;
	for(i = 0; i< column * (row + 1); i++)
	{
		if(send(sfd, presult[i], 128, 0) < 0)
		{
			ERR_MSG("send");

			int flag = 0;
			return -1;
		}
		printf("%s\t", presult[i]);
		trace();
	}

	return 0;
}

int do_back(char* current_user, cli_msg* info)
{
	printf("[%d] staff back to mainmenu [%s]\n", __LINE__, current_user);
	int sfd = info->newfd;

	char* errmsg = NULL;
	char sql[N] = "";
	sprintf(sql, "update staffSheet set STATE=0 where NAME=\"%s\";", current_user);
	if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
		return -1;
	}

	return 0;
}



int admin_delete(PACKAGE* packmsg, cli_msg* info)
{
	printf("[%d] admin delete [%s]\n", __LINE__, packmsg->name);

	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column,&errmsg) != 0)
	{
		printf("[%d]:sqlite3_get_table:%s\n",__LINE__,errmsg);
	}

	printf("%d,%d\n", column, row);

	int flag = 0;
	if(row == 0)
	{
		printf("no one named %s\n", packmsg->name);
		// 告知無
		if(send(sfd, &flag, sizeof(flag), 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}
	}
	else if(row != 0)
	{
		flag = 1;
		// 刪除此人
		bzero(sql, sizeof(sql));
		sprintf(sql, "delete from staffSheet where NAME=\"%s\";", packmsg->name);
		if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
			return -1;
		}
		if(send(sfd, &flag, sizeof(flag), 0) < 0)
		{
			ERR_MSG("send");
			return -1;
		}

		printf("[%d] : %s deleted \n", __LINE__, packmsg->name);
	}

	return 0;
}

int general_modify(PACKAGE* packmsg, cli_msg* info)
{
	printf("[%d] general modify [%s]\n", __LINE__, packmsg->name);
	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	if(recv(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
	{
		ERR_MSG("recv");
		return -1;
	}

	switch(packmsg->type)
	{
	case '1':
		printf("modify tel: [%s]\n", packmsg->tel);
		// 數據庫中 數據更改
		sprintf(sql, "update staffSheet set TEL=\"%s\" where NAME=\"%s\";", packmsg->tel, packmsg->name);
		break;
	case '2':
		printf("modify address: [%s]\n", packmsg->address);
		// 數據庫中 數據更改
		sprintf(sql, "update staffSheet set ADDRESS=\"%s\" where NAME=\"%s\";", packmsg->address, packmsg->name);
		break;
	case '3':
		printf("modify salary: [%.2f]\n", packmsg->salary);
		// 數據庫中 數據更改
		sprintf(sql, "update staffSheet set SALARY=\"%f\" where NAME=\"%s\";", packmsg->salary, packmsg->name);
		break;

	default:;

	}
	if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
		return -1;
		}
		else
		{
			printf("已更改信息\n");
		}


	return 0;
}


int admin_modify(PACKAGE* packmsg, cli_msg* info)
{

	printf("[%d] admin modify [%s]\n", __LINE__, packmsg->name);
	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	// 判斷是否存在此人
	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column,&errmsg) != 0)
	{
		printf("[%d]:sqlite3_get_table:%s\n",__LINE__,errmsg);
	}

	printf("%d,%d\n", column, row);
	if(send(sfd, &column, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	if(send(sfd, &row, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	bzero(sql, sizeof(sql));
	if(row == 0)
	{
		printf("no one named %s\n", packmsg->name);
	}
	else
	{
		if(recv(sfd, packmsg, sizeof(PACKAGE), 0) < 0)
		{
			ERR_MSG("recv");
			return -1;
		}

		switch(packmsg->type)
		{
		case '1':
			printf("modify tel: [%s]\n", packmsg->tel);
			// 數據庫中 數據更改
			sprintf(sql, "update staffSheet set TEL=\"%s\" where NAME=\"%s\";", packmsg->tel, packmsg->name);
			break;
		case '2':
			printf("modify address: [%s]\n", packmsg->address);
			// 數據庫中 數據更改
			sprintf(sql, "update staffSheet set ADDRESS=\"%s\" where NAME=\"%s\";", packmsg->address, packmsg->name);
			break;
		case '3':
			printf("modify salary: [%.2f]\n", packmsg->salary);
			// 數據庫中 數據更改
			sprintf(sql, "update staffSheet set SALARY=\"%f\" where NAME=\"%s\";", packmsg->salary, packmsg->name);
			break;

		default:;

		}
		if(sqlite3_exec(info->db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
			return -1;
		}
		else
		{
			printf("已更改信息\n");
		}
	}
	return 0;

}



int admin_search(PACKAGE* packmsg, cli_msg* info)
{
	printf("[%d] admin search [%s]\n", __LINE__, packmsg->name);

	int sfd = info->newfd;

	int row, column;
	char* errmsg = NULL;
	char **presult = NULL;
	char sql[N] = "";

	sprintf(sql, "select * from staffSheet where NAME=\"%s\";", packmsg->name);
	if(sqlite3_get_table(info->db, sql, &presult, &row, &column,&errmsg) != 0)
	{
		printf("[%d]:sqlite3_get_table:%s\n",__LINE__,errmsg);
	}

	printf("%d,%d\n", column, row);
	if(row == 0)
	{
		printf("no one named %s\n", packmsg->name);
	}

	if(send(sfd, &column, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}
	if(send(sfd, &row, 4, 0) < 0)
	{
		ERR_MSG("send");
		return -1;
	}

	int i = 0;
	for(i = 0; i< column * (row + 1); i++)
	{
		if(send(sfd, presult[i], 128, 0) < 0)
		{
			ERR_MSG("send");

			int flag = 0;
			return -1;
		}
		printf("%s\t", presult[i]);
		trace();
	}

	return 0;
}








// 交互 callback
void* do_interactive(void* arg)
{
	pthread_detach(pthread_self());

	cli_msg *info = (cli_msg*)arg;
	sqlite3* db = info->db;
	int newfd = info->newfd;
//	struct sockaddr_in cin = info->cin;

//	PACKAGE packmsg;

	char name[20] = "";
//	char inform[64] = "";  // 接收選擇類型

	PACKAGE packmsg;

	while(1)
	{
#if 1
		int res = recv(newfd, &packmsg, sizeof(packmsg), 0);

//		printf("[%s : %d] newfd = %d choose:%d\n",inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), newfd, packmsg.type);
		if(res < 0)
		{
			ERR_MSG("recv");
			return NULL;
		}
		else if(res == 0)
		{
	//		if(name[0] == 0)
	//		{
				printf("%d, 客戶端退出、\n", __LINE__);
	//			break;
	//		}
	//		printf("異常退出\n");
	
			do_quit(info, &packmsg);

	//		printf("[%s,%d] newfd = %d CLIENT closed\n",inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), newfd);
			// 改變表格登錄狀態 1登錄 0未登錄
			char sql[N] = "";
			char* errmsg = NULL;
			sprintf(sql, "update staffSheet set STATE=0 where NAME=\"%s\";", name);
			if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
			{
				printf("[%d]:sqlite3_exec:%s \n", __LINE__, errmsg);
				return NULL;
			}
			break;
		}
		printf("%d : 接收客戶端數據包成功, [%c]\n", __LINE__, packmsg.type);
#endif
		int ret;

		char current_user[20];
		strcpy(current_user, packmsg.name);

		switch(packmsg.type)
		{
		case 'R':  //1
			printf("ser: 選擇註冊\n");
			do_register(&packmsg, info);
			break;
		case 'L':  //2
			printf("ser: 選擇登錄\n");
			do_login(&packmsg, info);
#if 0
			ret = is_admin(info, &packmsg);
			if(ret == 1)
			{
				printf("---admin---\n");
			}
			else if(ret == 0)
			{
				printf("----general---\n");
			}
			else
			{
				printf("error\n");
			}
#endif
			break;
		case 'Q':  
			printf("ser: 選擇退出\n");
			do_quit(info, &packmsg);
			break;
		case 'S':  
			printf("admin: 查看員工\n");
			admin_search(&packmsg, info);
			break;
		case 'M':
			printf("general: 查看自己\n");
			general_search(&packmsg, info);
			break;
		case 'D':
			printf("admin: 刪除員工\n");
			admin_delete(&packmsg, info);
			break;
		case 'B':
			printf("staff: 返回主菜單\n");
			do_back(current_user, info);
			break;
		case 'A':
			printf("admin: 修改員工信息\n");
			admin_modify(&packmsg, info);
			break;
		case 'X':
			printf("general: 修改自己信息\n");
			general_modify(&packmsg, info);
			break;
		case 'Z':
			printf("admin: 增加員工\n");
			admin_add(&packmsg, info);
			break;
		default:
			break;
		}
	}
	pthread_exit(NULL);
}




int main(int argc, const char *argv[])
{
    printf("-- WELCOME --\n");

    //外部传参 IP:argv[1], PORT:argv[2]
    if (argc < 3) {
        fprintf(stderr, "please enter IP & PORT number : \n");
        return -1;
	}

	// 员工staffDb 初始化
	sqlite3 *db = NULL;
	// 打开数据库
	if (sqlite3_open("./staffDb.db", &db) != 0) 
	{
		fprintf(stderr, "sqlite3_open: %s\n", sqlite3_errmsg(db));
		return -1;
	}
	printf("staffDb open successful\n");

	__staffDb_init__(db);

	// 网络配置
	// 創建套接字
	int sfd = __socket_init__(argv[1],atoi(argv[2]));
	if(sfd < 0)
	{
		printf("ERROR\n");
		return -1;
	}

	//多线程并发服务器，主线程管理连接，分之线程管理交互(通信)
	struct sockaddr_in cin;
	socklen_t addrlen = sizeof(cin);
	cli_msg info;

	int newfd = 0;
	while(1)
	{
		// accept 建立連接
		newfd = accept(sfd, (struct sockaddr*)&cin, &addrlen);
		if(newfd < 0)
		{
			ERR_MSG("accept");
			return -1;
		}
		printf("[%s:%d] newfd = %d connected >o< \n", (char*)inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), newfd);

		info.db = db;
		info.cin = cin;
		info.newfd = newfd;
		// 創建线程
		pthread_t tid;
		if (pthread_create(&tid, NULL, do_interactive, (void*)&info) != 0)
		{
			ERR_MSG("pthread_create");
			return -1;
		}
	}

	return 0;
}
