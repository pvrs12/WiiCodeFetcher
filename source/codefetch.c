#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <network.h>
#include <fat.h>

#include <wiiuse/wpad.h>

const int SCREEN_TOP = 2;
const int SCREEN_BOT = 38;
const int SCREEN_LEFT= 0;
const int SCREEN_RIGHT = 80;

static void* xfb = NULL;

void* initialise();
static GXRModeObj* rmode = NULL;

const int min_row = 0;
const int min_col = 0;
const int max_row = 3;
const int max_col = 9;

static int cursorrow = 0;
static int cursorcol = 0;
static char cursorchar = 0;

const int maxIdLen = 9;
static int idlen = 0;
static char id[10];

const static char SERVER[] = "www.geckocodes.org";
const static int PORT = 80;
const static char REQUEST[] = "GET /txt.php?txt=%s HTTP/1.0\n\
Host: www.geckocodes.org\n\
Accept: */*\n\
\n";
const static char RESP_OK[] = "HTTP/1.1 200 OK";

void download_code(){
	//clear screen
	printf("\033[2J");
	printf("\n\nConfiguring network...\n");

	s32 ret;

	char localip[16] = {0};
	char gateway[16] = {0};
	char netmask[16] = {0};

	ret = if_config(localip,netmask,gateway,TRUE);
	if(ret>=0) {
		printf("network configured, ip: %s\n",localip);
	} else {
		printf("Network failed. Quitting...");
		exit(0);
	}
	printf("\nDownloading codes for %s...\n",id);

	struct hostent* hostp;
	struct sockaddr_in serveraddr;

	s32 sock;
	sock = net_socket(AF_INET,SOCK_STREAM,0);
	if (sock == INVALID_SOCKET){
		printf("Cannot create socket!\n");
		exit(0);
	}
	memset(&serveraddr, 0x00,sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(PORT);
	hostp = net_gethostbyname(SERVER);
	if(hostp == (struct hostent* )NULL){
		printf("Host not found!\n");
		exit(0);
	}
	//copy the address from the looked up host to serveraddr
	memcpy(&serveraddr.sin_addr,hostp->h_addr,sizeof(serveraddr.sin_addr));

	//connect to the server
	ret = net_connect(sock,(struct sockaddr*) &serveraddr,sizeof(serveraddr));
	if(ret<0){
		printf("Connect failed!\n");
		net_close(sock);
		exit(0);
	}

	char buff[512];
	sprintf(buff,REQUEST,id);
	int bufflen = strlen(buff);
	ret = net_send(sock,buff,bufflen,0);
	if(ret < 0){
		printf("Write failed!\n");
		net_close(sock);
		exit(0);
	}

	char codebuff[20];
	int okaylen = strlen(RESP_OK);
	ret = net_recv(sock,codebuff,okaylen,0);
	if(strncmp(codebuff,RESP_OK,okaylen)!=0){
		printf("Could not find any codes for %s!\n",id);
	//	printf("Server responded with \"%s\"\n",codebuff);
		return;
	}

	char filename[50];
	sprintf(filename,"/txtcodes/%s.txt",id);
	printf("Found codes! Downloading to %s...\n",filename);

	if(!fatInitDefault()){
		printf("Initialize Filesystem failed!\n");
		exit(0);
	}
	FILE* f = fopen(filename,"w");
	
	int output = FALSE;
	while((ret = net_recv(sock,&buff,512,0))>0){
		char* body;
		if (output){
			fprintf(f,"%.*s",ret,buff);
		} else {
			body = strstr(buff,"\r\n\r\n");
		}
		if (!output && body){
			output = TRUE;
			fprintf(f,"%.*s",ret,body);
		}
	}
	if(ret < 0){
		printf("Read failed! \n");
		fclose(f);
		net_close(sock);
		exit(0);
	} 
	fclose(f);
	net_close(sock);
	printf("\nDone downloading codes!\n");
	return;	
//	exit(0);
}

void draw_keyboard(){
	printf("\n");
	//save the cursor position
	printf("\033[s");
	cursorrow = 0;
	cursorcol = 0;
	cursorchar = 'A';
	int i;
	char c = 'A';
	for(i=0;i<36;++i,++c){
		if(i >= 26 && c > '9'){
			c='0';
		}
		if(i % 10 == 0 && i != 0){
			//move to the next line (twice)
			printf("\n");
			printf("\n");
		}
		printf("%c ",c);
	}
}

enum direction {
	UP,LEFT,DOWN,RIGHT
};

void move_cursor(enum direction d){
	if (d == UP){
		if(cursorrow > min_row){
			//move cursor back
			printf("\033[1D");
			//print space
			printf(" ");
			cursorrow--;
			cursorchar-=10;
			//move up 2 lines
			printf("\033[2A");
			//back 1
			printf("\033[1D");
			printf("^");
		}
	} else if (d == LEFT){
		if(cursorcol > min_col){
			printf("\033[1D");
			printf(" ");
			cursorcol--;
			cursorchar -=1;
			//back 3
			printf("\033[3D");
			printf("^");
		}
	} else if (d == RIGHT){
		if(cursorcol < max_col){
			printf("\033[1D");
			printf(" ");
			cursorcol++;
			cursorchar +=1;
			//forward 1
			printf("\033[1C");
			printf("^");
		}
	} else if (d == DOWN){
		if(cursorrow < max_row){
			printf("\033[1D");
			printf(" ");
			cursorrow++;
			cursorchar += 10;
			//move down 2 lines
			printf("\033[2B");
			//backward 1
			printf("\033[1D");
			printf("^");
		}
	}
}

/** will appear on the next line */
void draw_keyboard_pos(int topRow, int topCol){
	printf("\033[%d;%dH",topRow,topCol);
	draw_keyboard();
}

int main(int argc, char** argv){
	xfb = initialise();
	
	printf("\n\n\nPatrick's Code Fetcher!\n");
	printf("It ain't pretty!\n\n");

	printf("Press any key to type in a game ID\n");
	printf("Press HOME to quit\n");

	int typing = FALSE;

	while(1) {
		VIDEO_WaitVSync();
		WPAD_ScanPads();

		int buttonsDown = WPAD_ButtonsDown(0);

		if(buttonsDown & WPAD_BUTTON_HOME){
			exit(0);
		} else if(buttonsDown && !typing){
			printf("\033[2J");
			printf("\n\n");
			printf("Use DPAD to move around, press A to type a letter, B to backspace, 1 to submit\n");
			draw_keyboard();
			//goto 'A'
			printf("\033[u");
			//move down 1
			printf("\033[1B");
			//print underscore
			printf("^");
			typing = TRUE;
		} else if(buttonsDown & WPAD_BUTTON_A){
			if (idlen <= maxIdLen) {
				//save the cursor position
				printf("\033[s");
				int number =-1;
				if(cursorchar > 'Z'){
					number = (cursorchar-'Z' - 1);
				}
				//goto the bottom
				printf("\033[%d;%dH",SCREEN_BOT-10,SCREEN_LEFT+1+idlen);
				char let[2] = {0};
				if(number != -1){
					if(number < 10){
						printf("%d",number);
						sprintf(let,"%d",number);
					}
				} else {
					printf("%c",cursorchar);
					sprintf(let,"%c",cursorchar);
				}
				if(number < 10){
					idlen+=1;
					strcat(id,let);
				}
				//return the cursor
				printf("\033[u");
			}
		} else if(buttonsDown & WPAD_BUTTON_UP){
			if(typing){
				move_cursor(UP);
			}
		} else if(buttonsDown & WPAD_BUTTON_LEFT){
			if(typing){
				move_cursor(LEFT);
			}
		} else if(buttonsDown & WPAD_BUTTON_DOWN){
			if(typing){
				move_cursor(DOWN);
			}
		} else if(buttonsDown & WPAD_BUTTON_RIGHT){
			if(typing){
				move_cursor(RIGHT);
			}
		} else if(buttonsDown & WPAD_BUTTON_B){
			if(typing){
				if(idlen > 0){
					printf("\033[s");
					printf("\033[%d;%dH",SCREEN_BOT-10,SCREEN_LEFT+idlen);
					printf(" ");
					printf("\033[u");
					idlen--;
					id[idlen] = '\0';
				}
			}
		} else if(buttonsDown & WPAD_BUTTON_1){
			if(typing){
				typing = FALSE;
				printf("\033[%d;%dH",SCREEN_BOT-10,SCREEN_LEFT+idlen+1);
				printf("\n\n");
				download_code();

				//clear screen, and restart
				printf("Press any key to continue...\n");
				int waiting = TRUE;
				while(waiting){
					VIDEO_WaitVSync();
					WPAD_ScanPads();

					int buttonsDown = WPAD_ButtonsDown(0);
					if(buttonsDown){
						waiting = FALSE;
					}
				}

				strcpy(id,"\0");
				idlen = 0;
				printf("\033[2J");
				printf("\n\n");
				printf("Use DPAD to move around, press A to type a letter, B to backspace, 1 to submit\n");
				draw_keyboard();
				//goto 'A'
				printf("\033[u");
				//move down 1
				printf("\033[1B");
				//print underscore
				printf("^");
				typing = TRUE;
			}
		}
	}
}

void* initialise() {
//---------------------------------------------------------------------------------

	void *framebuffer;

	VIDEO_Init();
	WPAD_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);
	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(framebuffer,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	return framebuffer;

}
