
#define NULL ((void*)0)
/* Mayura OS v1.0 - by Susanth
   CLI-first bare metal kernel | SimpleC interpreter | GUI mode
*/
#include <stdint.h>

/* ═══ I/O ══════════════════════════════════════════════════ */
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t r;__asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));return r;}
static inline void io_wait(void){outb(0x80,0);}

/* ═══ VGA ═══════════════════════════════════════════════════ */
#define VW 80
#define VH 25
static volatile uint16_t*const VGA=(uint16_t*)0xB8000;
#define BLK 0
#define BLU 1
#define GRN 2
#define CYN 3
#define RED 4
#define MAG 5
#define BRN 6
#define LGR 7
#define DGR 8
#define LBL 9
#define LGN 10
#define LCY 11
#define LRD 12
#define PNK 13
#define YLW 14
#define WHT 15
static uint8_t AT(uint8_t f,uint8_t b){return f|(b<<4);}
static void vp(int r,int c,char ch,uint8_t a){if(r>=0&&r<VH&&c>=0&&c<VW)VGA[r*VW+c]=(uint16_t)(uint8_t)ch|((uint16_t)a<<8);}
static void vf(int r,int c,int n,char ch,uint8_t a){for(int i=0;i<n;i++)vp(r,c+i,ch,a);}
static void vr_fill(int r,int c,int h,int w,char ch,uint8_t a){for(int i=0;i<h;i++)vf(r+i,c,w,ch,a);}
static void vt(int r,int c,const char*s,uint8_t a){while(*s&&c<VW)vp(r,c++,*s++,a);}
static int vslen(const char*s){int n=0;while(s[n])n++;return n;}
static void vc_str(int r,int cl,int w,const char*s,uint8_t a){int x=cl+(w-vslen(s))/2;if(x<cl)x=cl;vt(r,x,s,a);}
static void vi(int r,int c,int n,uint8_t a){char b[16];int i=15;b[i]=0;int neg=n<0;if(neg)n=-n;if(!n)b[--i]='0';else while(n>0){b[--i]='0'+n%10;n/=10;}if(neg)b[--i]='-';vt(r,c,b+i,a);}
static void vcls_color(uint8_t a){for(int i=0;i<VW*VH;i++)VGA[i]=(uint16_t)' '|((uint16_t)a<<8);}
static void vcls(void){vcls_color(AT(LGR,BLK));}
static void cur_show(int r,int c){uint16_t p=r*VW+c;outb(0x3D4,14);outb(0x3D5,p>>8);outb(0x3D4,15);outb(0x3D5,p&0xFF);outb(0x3D4,0x0A);outb(0x3D5,14);outb(0x3D4,0x0B);outb(0x3D5,15);}
static void cur_hide(void){outb(0x3D4,0x0A);outb(0x3D5,0x20);}

/* ═══ Strings ════════════════════════════════════════════════ */
static void kms(void*d,int v,int n){uint8_t*p=d;while(n--)*p++=v;}
static void kmc(void*d,const void*s,int n){uint8_t*p=d;const uint8_t*q=s;while(n--)*p++=*q++;}
static int ksl(const char*s){int n=0;while(s[n])n++;return n;}
static void kss(char*d,const char*s){while((*d++=*s++));}
static int ksc(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a-*b;}
static int knc(const char*a,const char*b,int n){while(n&&*a&&*b&&*a==*b){a++;b++;n--;}return n?(*a-*b):0;}
static void ksi(char*d,int n){char b[16];int i=15;b[i]=0;int neg=n<0;if(neg)n=-n;if(!n)b[--i]='0';else while(n>0){b[--i]='0'+n%10;n/=10;}if(neg)b[--i]='-';kss(d,b+i);}
static int katoi(const char*s){int n=0,neg=0;if(*s=='-'){neg=1;s++;}while(*s>='0'&&*s<='9'){n=n*10+(*s++-'0');}return neg?-n:n;}
static void kupper(char*s){while(*s){if(*s>='a'&&*s<='z')*s-=32;s++;}}
static void klower(char*s){while(*s){if(*s>='A'&&*s<='Z')*s+=32;s++;}}

/* ═══ IDT ════════════════════════════════════════════════════ */
struct idt_e{uint16_t lo,sel;uint8_t z,f;uint16_t hi;}__attribute__((packed));
struct idt_p{uint16_t lim;uint32_t base;}__attribute__((packed));
static struct idt_e IDT[256];
static struct idt_p IDTP;
extern void isr_noerr(void);extern void isr_err(void);extern void isr_irq(void);
extern void isr_timer(void);extern void isr_keyboard(void);
static void idt_set(int n,uint32_t b){IDT[n].lo=b&0xFFFF;IDT[n].hi=b>>16;IDT[n].sel=0x08;IDT[n].z=0;IDT[n].f=0x8E;}

/* ═══ PIC ════════════════════════════════════════════════════ */
static void pic_init(void){
    outb(0x20,0x11);outb(0xA0,0x11);io_wait();
    outb(0x21,0x20);outb(0xA1,0x28);io_wait();
    outb(0x21,0x04);outb(0xA1,0x02);io_wait();
    outb(0x21,0x01);outb(0xA1,0x01);io_wait();
    outb(0x21,0xFC);/* IRQ0+IRQ1 only */
    outb(0xA1,0xFF);/* mask all slave */
}
static void eoi(int irq){if(irq>=8)outb(0xA0,0x20);outb(0x20,0x20);}

/* ═══ Timer ══════════════════════════════════════════════════ */
static volatile uint32_t ticks=0;
void timer_handler(void){ticks++;eoi(0);}
static void pit_init(void){outb(0x43,0x36);outb(0x40,0x9B);outb(0x40,0x2E);}
static void msleep(uint32_t ms){uint32_t t=ticks+ms/10;while(ticks<t)__asm__("hlt");}

/* ═══ Keyboard ═══════════════════════════════════════════════ */
#define KBSZ 256
static volatile uint8_t kbuf[KBSZ];
static volatile uint8_t kh=0,kt=0;
static uint8_t kshift=0,ke0=0,kcaps=0,kctrl=0;
#define K_UP   0x80
#define K_DN   0x81
#define K_LT   0x82
#define K_RT   0x83
#define K_F1   0x84
#define K_F2   0x85
#define K_F3   0x86
#define K_F4   0x87
#define K_F5   0x88
#define K_F6   0x89
#define K_F7   0x8A
#define K_F8   0x8B
#define K_F9   0x8C
#define K_F10  0x8D
#define K_DEL  0x8E
#define K_HOME 0x8F
#define K_END  0x90
#define K_PGUP 0x91
#define K_PGDN 0x92
#define K_ESC  0x1B
#define K_BS   0x08
#define K_ENT  0x0D
#define K_TAB  0x09
static const char slo[58]={0,0x1B,'1','2','3','4','5','6','7','8','9','0','-','=',0x08,0x09,'q','w','e','r','t','y','u','i','o','p','[',']',0x0D,0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
static const char shi[58]={0,0x1B,'!','@','#','$','%','^','&','*','(',')','_','+',0x08,0x09,'Q','W','E','R','T','Y','U','I','O','P','{','}',0x0D,0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '};
static void kpush(uint8_t k){uint8_t nx=(kh+1)%KBSZ;if(nx!=kt){kbuf[kh]=k;kh=nx;}}
void keyboard_handler(void){
    uint8_t sc=inb(0x60);
    if(sc==0xE0){ke0=1;eoi(1);return;}
    if(sc&0x80){
        uint8_t rr=sc&0x7F;
        if(rr==0x2A||rr==0x36)kshift=0;
        if(rr==0x1D)kctrl=0;
        ke0=0;eoi(1);return;
    }
    if(ke0){ke0=0;
        switch(sc){case 0x48:kpush(K_UP);break;case 0x50:kpush(K_DN);break;case 0x4B:kpush(K_LT);break;case 0x4D:kpush(K_RT);break;case 0x53:kpush(K_DEL);break;case 0x47:kpush(K_HOME);break;case 0x4F:kpush(K_END);break;case 0x49:kpush(K_PGUP);break;case 0x51:kpush(K_PGDN);break;}
        eoi(1);return;
    }
    if(sc==0x1D){kctrl=1;eoi(1);return;}
    if(sc==0x3A){kcaps^=1;eoi(1);return;}
    if(sc==0x2A||sc==0x36){kshift=1;eoi(1);return;}
    if(sc==0x3B){kpush(K_F1);eoi(1);return;}if(sc==0x3C){kpush(K_F2);eoi(1);return;}
    if(sc==0x3D){kpush(K_F3);eoi(1);return;}if(sc==0x3E){kpush(K_F4);eoi(1);return;}
    if(sc==0x3F){kpush(K_F5);eoi(1);return;}if(sc==0x40){kpush(K_F6);eoi(1);return;}
    if(sc==0x41){kpush(K_F7);eoi(1);return;}if(sc==0x42){kpush(K_F8);eoi(1);return;}
    if(sc==0x43){kpush(K_F9);eoi(1);return;}if(sc==0x44){kpush(K_F10);eoi(1);return;}
    if(sc<58){int hi=kshift^(kcaps&&sc>=0x10&&sc<=0x32);char c=hi?shi[sc]:slo[sc];if(c)kpush((uint8_t)c);}
    eoi(1);
}
static int kpoll(uint8_t*k){if(kt==kh)return 0;*k=kbuf[kt];kt=(kt+1)%KBSZ;return 1;}
static uint8_t kgetc(void){uint8_t k;while(!kpoll(&k))__asm__("hlt");return k;}

/* ═══ RTC / IST ═══════════════════════════════════════════════ */
static uint8_t bcd(uint8_t b){return(b>>4)*10+(b&0xF);}
static void rtc_ist(uint8_t*h,uint8_t*m,uint8_t*s){
    outb(0x70,0x0A);while(inb(0x71)&0x80){}
    outb(0x70,4);*h=bcd(inb(0x71));outb(0x70,2);*m=bcd(inb(0x71));outb(0x70,0);*s=bcd(inb(0x71));
    uint16_t t=(*h)*60+(*m)+330;t%=1440;*h=t/60;*m=t%60;
}
static void rtc_date(uint8_t*day,uint8_t*mon,uint8_t*yr){
    outb(0x70,0x0A);while(inb(0x71)&0x80){}
    outb(0x70,7);*day=bcd(inb(0x71));outb(0x70,8);*mon=bcd(inb(0x71));outb(0x70,9);*yr=bcd(inb(0x71));
}

/* ═══ RNG ═════════════════════════════════════════════════════ */
static uint32_t rng=0xDEADBEEF;
static uint32_t rrand(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}

/* ═══ Filesystem ══════════════════════════════════════════════ */
#define FS_MAX 128
#define FS_NSIZ 32
#define FS_DSIZ 1024
typedef struct{
    char name[FS_NSIZ];
    char data[FS_DSIZ];
    int  size;
    int  parent;/* -1=root */
    int  is_dir;
    int  used;
}FSNode;
static FSNode fs[FS_MAX];
static int fs_cwd=0;

static int fs_alloc(void){for(int i=0;i<FS_MAX;i++)if(!fs[i].used)return i;return -1;}
static int fs_find(const char*name,int par){for(int i=0;i<FS_MAX;i++)if(fs[i].used&&fs[i].parent==par&&ksc(fs[i].name,name)==0)return i;return -1;}
static int fs_mkfile(const char*name,int par,const char*data){
    int n=fs_alloc();if(n<0)return -1;
    fs[n].used=1;fs[n].is_dir=0;fs[n].parent=par;
    kss(fs[n].name,name);
    if(data){int dl=ksl(data);if(dl>=FS_DSIZ)dl=FS_DSIZ-1;kmc(fs[n].data,data,dl);fs[n].size=dl;}
    else{fs[n].size=0;}
    return n;
}
static int fs_mkdir(const char*name,int par){
    int n=fs_alloc();if(n<0)return -1;
    fs[n].used=1;fs[n].is_dir=1;fs[n].parent=par;
    kss(fs[n].name,name);fs[n].size=0;
    return n;
}
static void fs_cwd_path(char*out){
    if(fs_cwd==0){out[0]='/';out[1]=0;return;}
    /* collect ancestors - walk up to root */
    int chain[16];int cc=0;int cur=fs_cwd;
    while(cur>0&&cc<16){chain[cc++]=cur;cur=fs[cur].parent;}
    /* build path forwards */
    int pos=0;
    for(int i=cc-1;i>=0;i--){
        out[pos++]='/';
        const char*nm=fs[chain[i]].name;
        for(int j=0;nm[j]&&pos<62;j++)out[pos++]=nm[j];
    }
    out[pos]=0;
    if(pos==0){out[0]='/';out[1]=0;}
}
static void fs_init(void){
    kms(fs,0,sizeof(fs));
    /* root */
    fs[0].used=1;fs[0].is_dir=1;kss(fs[0].name,"/");fs[0].parent=-1;
    /* /home */
    int home=fs_mkdir("home",0);
    int susanth=fs_mkdir("susanth",home);
    fs_mkfile("readme.txt",susanth,"Welcome to Mayura OS!\nCreated by Susanth.\nType 'help' for commands.\n");
    fs_mkfile("hello.sc",susanth,"// SimpleC hello world\nprintln(\"Hello from SimpleC!\");\nint x = 5;\nprintln(x * 2);\n");
    fs_mkfile(".bashrc",susanth,"# Mayura OS shell config\n# User: susanth\n");
    /* /etc */
    int etc=fs_mkdir("etc",0);
    fs_mkfile("hostname",etc,"mayura\n");
    fs_mkfile("motd",etc,"Welcome to Mayura OS v1.0\nCreated by Susanth\nType 'help' for commands\n");
    fs_mkfile("os-release",etc,"NAME=MayuraOS\nVERSION=1.0\nAUTHOR=Susanth\n");
    /* /tmp */
    fs_mkdir("tmp",0);
    /* /usr/share */
    int usr=fs_mkdir("usr",0);
    int share=fs_mkdir("share",usr);
    int fortune_dir=fs_mkdir("fortune",share);
    fs_mkfile("quotes",fortune_dir,"The best way to predict the future is to invent it.\nSimplicity is the soul of efficiency.\nAny sufficiently advanced technology is indistinguishable from magic.\nFirst, solve the problem. Then, write the code.\nCode is like humor. When you have to explain it, it's bad.\nThe computer was born to solve problems that did not exist before.\nTalk is cheap. Show me the code.\nPerfection is achieved not when there is nothing more to add.\n");
    /* /var/log */
    int var=fs_mkdir("var",0);
    int log=fs_mkdir("log",var);
    fs_mkfile("kern.log",log,"[    0.000000] Mayura OS kernel initializing\n[    0.001234] Memory initialized\n[    0.002000] IDT loaded\n[    0.003000] PIC initialized\n[    0.004000] PIT timer started\n[    0.005000] Keyboard driver loaded\n[    0.010000] Filesystem initialized\n[    1.000000] Boot complete\n");
    /* /bin */
    int bin=fs_mkdir("bin",0);
    fs_mkfile("README",bin,"This directory contains built-in commands.\nRun 'help' to see all available commands.\n");
    /* start in home/susanth */
    fs_cwd=susanth;
}

/* ═══ Terminal output ════════════════════════════════════════ */
static int term_row=0,term_col=0;
static uint8_t term_attr=LGR|(BLK<<4);
static int line_input_row=0;
/* Scrollback buffer: last 200 lines */
#define SCROLL_LINES 200
#define SCROLL_COLS  80
static char scroll_buf[SCROLL_LINES][SCROLL_COLS+1];
static uint8_t scroll_attr[SCROLL_LINES][SCROLL_COLS];
static int scroll_head=0;/* next write position */
static int scroll_count=0;/* how many lines stored */
static int scroll_view=0;/* 0=bottom (normal), positive=scrolled up */

static void scroll_save_line(int row){
    /* save current VGA row to scrollback */
    for(int c=0;c<SCROLL_COLS;c++){
        scroll_buf[scroll_head][c]=(char)(VGA[row*VW+c]&0xFF);
        scroll_attr[scroll_head][c]=(uint8_t)(VGA[row*VW+c]>>8);
    }
    scroll_buf[scroll_head][SCROLL_COLS]=0;
    scroll_head=(scroll_head+1)%SCROLL_LINES;
    if(scroll_count<SCROLL_LINES)scroll_count++;
}
static void scroll_page_up(void){
    if(scroll_view+VH<=scroll_count){
        scroll_view+=VH/2;
        /* redraw from scrollback */
        vcls_color(AT(LGR,BLK));
        int available=scroll_count-scroll_view;
        int show=available<VH-1?available:VH-1;
        for(int r=0;r<show;r++){
            int idx=(scroll_head-scroll_count+scroll_view+r+SCROLL_LINES*2)%SCROLL_LINES;
            for(int c=0;c<SCROLL_COLS;c++)
                VGA[r*VW+c]=(uint16_t)(uint8_t)scroll_buf[idx][c]|((uint16_t)scroll_attr[idx][c]<<8);
        }
        vf(VH-1,0,VW,' ',AT(BLK,LGR));
        vt(VH-1,2,"[SCROLLBACK - PgDn to return]",AT(YLW,LGR));
    }
}
static void scroll_page_down(void){
    if(scroll_view>0){scroll_view-=VH/2;if(scroll_view<0)scroll_view=0;}
    if(scroll_view==0){/* restore normal view */scroll_view=0;}
}

static void term_scroll(void){
    /* save top line to scrollback before it disappears */
    scroll_save_line(0);
    for(int r=1;r<VH;r++)
        for(int c=0;c<VW;c++)
            VGA[(r-1)*VW+c]=VGA[r*VW+c];
    vf(VH-1,0,VW,' ',term_attr);
    if(term_row>0)term_row--;
    if(line_input_row>0)line_input_row--;
}
static void term_putc(char c){
    if(c=='\n'){term_col=0;term_row++;if(term_row>=VH)term_scroll();return;}
    if(c=='\r'){term_col=0;return;}
    if(c=='\t'){int ns=(term_col+8)&~7;if(ns>=VW)ns=VW-1;while(term_col<ns)vp(term_row,term_col++,' ',term_attr);return;}
    if(c=='\b'){if(term_col>0){term_col--;vp(term_row,term_col,' ',term_attr);}return;}
    vp(term_row,term_col++,c,term_attr);
    if(term_col>=VW){term_col=0;term_row++;if(term_row>=VH)term_scroll();}
}
static void term_puts(const char*s){while(*s)term_putc(*s++);}
static void term_puti(int n){char b[16];ksi(b,n);term_puts(b);}
static void term_putcolor(const char*s,uint8_t a){uint8_t old=term_attr;term_attr=a;term_puts(s);term_attr=old;}
static void term_nl(void){term_putc('\n');}
static void term_clear(void){vcls();term_row=0;term_col=0;}

/* ═══ Shell input line ════════════════════════════════════════ */
#define LINE_MAX 256
#define HIST_MAX 50
static char hist[HIST_MAX][LINE_MAX];
static int hist_count=0,hist_pos=0;
static char line_buf[LINE_MAX];
static int line_len=0,line_cur=0;

/* autocomplete list */
static const char*CMDS[]={
    "ls","cd","pwd","mkdir","rmdir","touch","rm","cp","mv","cat","head","tail",
    "more","wc","grep","find","du","stat","tree","file","ln","tee","truncate",
    "echo","printf","clear","reset","banner","figlet","rev","upper","lower","rot13",
    "uname","uptime","date","time","whoami","hostname","ps","kill","env","history",
    "alias","reboot","shutdown","halt","dmesg","free","df","lscpu","lsmod",
    "nano","calc","clock","top","piano","snake","tetris","pong","2048","mines",
    "hang","ttt","guess","todo","timer","morse","type","quiz","math","weather",
    "sc","run","gui","help","man","neofetch","fortune","cowsay","sl","matrix",
    "fire","starfield","hack","about","credits","sudo","git","python","vim",
    "konami","om","clear","version","whoami","id","groups",
    (const char*)0
};

static void line_redraw(int prompt_len){
    int r=line_input_row;
    int c=prompt_len;
    if(r<0)r=0;if(r>=VH)r=VH-1;
    if(c<0)c=0;if(c>=VW-1)c=VW-10;/* safety cap */
    int avail=VW-c;if(avail<1)avail=1;
    vf(r,c,avail,' ',term_attr);
    for(int i=0;i<line_len&&c+i<VW;i++)vp(r,c+i,line_buf[i],term_attr);
    int cx=c+line_cur;if(cx>=VW)cx=VW-1;
    cur_show(r,cx);
}

static int line_read(const char*prompt,int prompt_len){
    line_len=0;line_cur=0;line_buf[0]=0;
    hist_pos=hist_count;
    line_input_row=term_row;/* save input row */
    cur_show(line_input_row,term_col);
    while(1){
        uint8_t k=kgetc();
        if(k==K_ENT||k=='\r'){
            term_nl();
            /* save to history */
            if(line_len>0){
                kms(hist[hist_count%HIST_MAX],0,LINE_MAX);
                kmc(hist[hist_count%HIST_MAX],line_buf,line_len);
                hist_count++;
            }
            return line_len;
        }
        if(k==K_ESC){line_len=0;line_cur=0;line_buf[0]=0;line_redraw(prompt_len);continue;}
        if(k==K_BS){
            if(line_cur>0){
                for(int i=line_cur-1;i<line_len-1;i++)line_buf[i]=line_buf[i+1];
                line_len--;line_cur--;line_buf[line_len]=0;
                line_redraw(prompt_len);
            }
            continue;
        }
        if(k==K_DEL){
            if(line_cur<line_len){
                for(int i=line_cur;i<line_len-1;i++)line_buf[i]=line_buf[i+1];
                line_len--;line_buf[line_len]=0;
                line_redraw(prompt_len);
            }
            continue;
        }
        if(k==K_LT){if(line_cur>0){line_cur--;cur_show(line_input_row,prompt_len+line_cur);}continue;}
        if(k==K_RT){if(line_cur<line_len){line_cur++;cur_show(line_input_row,prompt_len+line_cur);}continue;}
        if(k==K_HOME){line_cur=0;cur_show(line_input_row,prompt_len);continue;}
        if(k==K_END){line_cur=line_len;cur_show(line_input_row,prompt_len+line_len);continue;}
        if(k==K_PGUP){scroll_page_up();continue;}
        if(k==K_PGDN){scroll_page_down();continue;}
        if(k==K_UP){
            if(hist_pos>0&&hist_pos>hist_count-HIST_MAX){
                hist_pos--;
                int idx=hist_pos%HIST_MAX;
                kss(line_buf,hist[idx]);line_len=ksl(line_buf);line_cur=line_len;
                line_redraw(prompt_len);
            }
            continue;
        }
        if(k==K_DN){
            if(hist_pos<hist_count){
                hist_pos++;
                if(hist_pos==hist_count){line_len=0;line_buf[0]=0;line_cur=0;}
                else{int idx=hist_pos%HIST_MAX;kss(line_buf,hist[idx]);line_len=ksl(line_buf);line_cur=line_len;}
                line_redraw(prompt_len);
            }
            continue;
        }
        if(k==K_TAB){
            /* simple autocomplete */
            if(line_len>0){
                for(int i=0;CMDS[i]!=(const char*)0;i++){
                    if(knc(CMDS[i],line_buf,line_len)==0&&ksl(CMDS[i])>line_len){
                        kss(line_buf,CMDS[i]);line_len=ksl(line_buf);line_cur=line_len;
                        line_redraw(prompt_len);break;
                    }
                }
            }
            continue;
        }
        if(k>=' '&&k<0x80&&line_len<LINE_MAX-1){
            for(int i=line_len;i>line_cur;i--)line_buf[i]=line_buf[i-1];
            line_buf[line_cur++]=k;line_len++;line_buf[line_len]=0;
            line_redraw(prompt_len);
        }
    }
}

/* ═══ Command parser ══════════════════════════════════════════ */
#define MAX_ARGS 16
static char cmd_argv[MAX_ARGS][64];
static int cmd_argc=0;

static void parse_cmd(void){
    cmd_argc=0;
    int i=0;
    while(line_buf[i]==' ')i++;
    while(line_buf[i]&&cmd_argc<MAX_ARGS){
        int j=0;
        while(line_buf[i]&&line_buf[i]!=' '&&j<63){cmd_argv[cmd_argc][j++]=line_buf[i++];}
        cmd_argv[cmd_argc][j]=0;
        cmd_argc++;
        while(line_buf[i]==' ')i++;
    }
}

/* ═══ Box drawing removed ════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════
   SHELL COMMANDS
═══════════════════════════════════════════════════════════════ */

/* ── ls ─────────────────────────────────── */
static void cmd_ls(int flag_l){
    for(int i=0;i<FS_MAX;i++){
        if(!fs[i].used||fs[i].parent!=fs_cwd)continue;
        if(ksc(fs[i].name,"/")==0)continue;
        if(flag_l){
            term_putcolor(fs[i].is_dir?"d":"-",AT(LGR,BLK));
            term_puts(fs[i].is_dir?"rwxr-xr-x":"rw-r--r--");
            term_puts("  susanth  susanth  ");
            char sz[12];ksi(sz,fs[i].size);
            int pad=6-ksl(sz);while(pad-->0)term_putc(' ');
            term_puts(sz);term_putc(' ');
            if(fs[i].is_dir)term_putcolor(fs[i].name,AT(LBL,BLK));
            else term_putcolor(fs[i].name,AT(LGR,BLK));
            if(fs[i].is_dir)term_putc('/');
            term_nl();
        }else{
            if(fs[i].is_dir)term_putcolor(fs[i].name,AT(LBL,BLK));
            else term_puts(fs[i].name);
            if(fs[i].is_dir)term_putc('/');
            term_puts("  ");
        }
    }
    if(!flag_l)term_nl();
}

/* ── tree ───────────────────────────────── */
static void cmd_tree_r(int node,int depth){
    for(int i=0;i<FS_MAX;i++){
        if(!fs[i].used||fs[i].parent!=node)continue;
        for(int d=0;d<depth;d++)term_puts(d==depth-1?"\xC0\xC4\xC4 ":"    ");
        if(fs[i].is_dir)term_putcolor(fs[i].name,AT(LBL,BLK));
        else term_puts(fs[i].name);
        if(fs[i].is_dir){term_putc('/');term_nl();cmd_tree_r(i,depth+1);}
        else term_nl();
    }
}

/* ── cat ────────────────────────────────── */
static void cmd_cat(const char*name){
    int n=fs_find(name,fs_cwd);
    if(n<0){term_puts("cat: ");term_puts(name);term_puts(": No such file\n");return;}
    if(fs[n].is_dir){term_puts("cat: ");term_puts(name);term_puts(": Is a directory\n");return;}
    term_puts(fs[n].data);
    if(fs[n].size>0&&fs[n].data[fs[n].size-1]!='\n')term_nl();
}

/* ── grep ───────────────────────────────── */
static void cmd_grep(const char*pat,const char*fname){
    int n=fs_find(fname,fs_cwd);
    if(n<0){term_puts("grep: ");term_puts(fname);term_puts(": not found\n");return;}
    char line[80];int li=0;int pl=ksl(pat);
    for(int fi=0;fi<=fs[n].size;fi++){
        if(fi==fs[n].size||fs[n].data[fi]=='\n'){
            line[li]=0;
            for(int k=0;line[k];k++)if(knc(line+k,pat,pl)==0){term_puts(line);term_nl();break;}
            li=0;
        }else if(li<79)line[li++]=fs[n].data[fi];
    }
}

/* ── wc ─────────────────────────────────── */
static void cmd_wc(const char*name){
    int n=fs_find(name,fs_cwd);
    if(n<0){term_puts("wc: not found\n");return;}
    int lines=0,words=0,chars=fs[n].size,inw=0;
    for(int i=0;i<fs[n].size;i++){
        char c=fs[n].data[i];
        if(c=='\n')lines++;
        if(c==' '||c=='\n'||c=='\t')inw=0;else if(!inw){words++;inw=1;}
    }
    term_puti(lines);term_putc(' ');term_puti(words);term_putc(' ');term_puti(chars);term_putc(' ');term_puts(name);term_nl();
}

/* ── find ───────────────────────────────── */
static void cmd_find_r(int node,const char*pat,const char*prefix){
    for(int i=0;i<FS_MAX;i++){
        if(!fs[i].used||fs[i].parent!=node)continue;
        char path[64];kss(path,prefix);kss(path+ksl(path),"/");kss(path+ksl(path),fs[i].name);
        if(!ksl(pat)||knc(fs[i].name,pat,ksl(pat))==0)term_puts(path),term_nl();
        if(fs[i].is_dir)cmd_find_r(i,pat,path);
    }
}

/* ── nano editor ────────────────────────── */
#define NANO_ROWS 20
#define NANO_COLS 76
static char nano_buf[NANO_ROWS][NANO_COLS+1];
static int nano_elen[NANO_ROWS];
static int nano_row=0,nano_col=0,nano_mod=0;
static char nano_file[32];

static void nano_load(const char*name){
    kms(nano_buf,0,sizeof(nano_buf));kms(nano_elen,0,sizeof(nano_elen));
    nano_row=0;nano_col=0;nano_mod=0;kss(nano_file,name);
    int n=fs_find(name,fs_cwd);if(n<0)return;
    int row=0,col=0;
    for(int i=0;i<fs[n].size&&row<NANO_ROWS;i++){
        char c=fs[n].data[i];
        if(c=='\n'){nano_elen[row]=col;nano_buf[row][col]=0;row++;col=0;}
        else if(col<NANO_COLS)nano_buf[row][col++]=c;
    }
    nano_elen[row]=col;nano_buf[row][col]=0;
}
static void nano_save(void){
    int n=fs_find(nano_file,fs_cwd);
    if(n<0){n=fs_alloc();if(n<0)return;fs[n].used=1;fs[n].is_dir=0;kss(fs[n].name,nano_file);fs[n].parent=fs_cwd;}
    int p=0;
    for(int r=0;r<NANO_ROWS&&p<FS_DSIZ-2;r++){
        for(int c=0;c<nano_elen[r]&&p<FS_DSIZ-2;c++)fs[n].data[p++]=nano_buf[r][c];
        if(p<FS_DSIZ-2)fs[n].data[p++]='\n';
    }
    fs[n].size=p;nano_mod=0;
}
static void nano_run(const char*filename){
    nano_load(filename);
    cur_hide();
    while(1){
        /* draw */
        vcls_color(AT(WHT,BLK));
        /* header */
        vf(0,0,VW,' ',AT(BLK,CYN));
        vc_str(0,0,VW,"GNU nano  (Mayura OS)",AT(BLK,CYN));
        vt(0,2,nano_file,AT(BLK,CYN));
        if(nano_mod)vt(0,VW-12,"[Modified]",AT(YLW,CYN));
        /* body */
        for(int r=0;r<NANO_ROWS;r++){
            uint8_t a=(r==nano_row)?AT(BLK,LGR):AT(WHT,BLK);
            vf(r+1,0,VW,' ',a);
            vp(r+1,0,'0'+(r+1)/10,AT(DGR,a>>4));
            vp(r+1,1,'0'+(r+1)%10,AT(DGR,a>>4));
            vp(r+1,2,'|',AT(DGR,a>>4));
            vt(r+1,3,nano_buf[r],a);
        }
        /* footer */
        vf(22,0,VW,' ',AT(BLK,CYN));
        vt(22,0,"^X Exit  ^S Save  Arrows Move  Del Backspace",AT(BLK,CYN));
        vf(23,0,VW,' ',AT(BLK,CYN));
        char pos[32];kss(pos,"Ln:");ksi(pos+ksl(pos),nano_row+1);kss(pos+ksl(pos)," Col:");ksi(pos+ksl(pos),nano_col+1);
        vt(23,0,pos,AT(BLK,CYN));
        cur_show(nano_row+1,3+nano_col);
        /* input */
        uint8_t k=kgetc();
        if(kctrl&&k=='x'){break;}
        if(kctrl&&k=='s'){nano_save();continue;}
        if(k==K_UP&&nano_row>0){nano_row--;if(nano_col>nano_elen[nano_row])nano_col=nano_elen[nano_row];continue;}
        if(k==K_DN&&nano_row<NANO_ROWS-1){nano_row++;if(nano_col>nano_elen[nano_row])nano_col=nano_elen[nano_row];continue;}
        if(k==K_LT){if(nano_col>0)nano_col--;else if(nano_row>0){nano_row--;nano_col=nano_elen[nano_row];}continue;}
        if(k==K_RT){if(nano_col<nano_elen[nano_row])nano_col++;else if(nano_row<NANO_ROWS-1){nano_row++;nano_col=0;}continue;}
        if(k==K_HOME){nano_col=0;continue;}
        if(k==K_END){nano_col=nano_elen[nano_row];continue;}
        if(k==K_ENT&&nano_row<NANO_ROWS-1){nano_row++;nano_col=0;nano_mod=1;continue;}
        if(k==K_BS&&nano_col>0){for(int i=nano_col-1;i<nano_elen[nano_row]-1;i++)nano_buf[nano_row][i]=nano_buf[nano_row][i+1];nano_elen[nano_row]--;nano_buf[nano_row][nano_elen[nano_row]]=0;nano_col--;nano_mod=1;continue;}
        if(k>=' '&&k<0x80&&nano_elen[nano_row]<NANO_COLS){for(int i=nano_elen[nano_row];i>nano_col;i--)nano_buf[nano_row][i]=nano_buf[nano_row][i-1];nano_buf[nano_row][nano_col]=k;nano_elen[nano_row]++;nano_buf[nano_row][nano_elen[nano_row]]=0;nano_col++;nano_mod=1;}
    }
    vcls();term_row=0;term_col=0;
}

/* ── calculator ─────────────────────────── */
static int calc_eval_expr(const char*s,int*p);
static int calc_num(const char*s,int*p){int n=0,neg=0;while(s[*p]==' ')(*p)++;if(s[*p]=='-'){neg=1;(*p)++;}while(s[*p]>='0'&&s[*p]<='9')n=n*10+(s[(*p)++]-'0');return neg?-n:n;}
static int calc_eval_expr(const char*s,int*p){while(s[*p]==' ')(*p)++;int a;if(s[*p]=='('){(*p)++;a=calc_eval_expr(s,p);if(s[*p]==')')(*p)++;}else a=calc_num(s,p);while(1){while(s[*p]==' ')(*p)++;char op=s[*p];if(op!='+'&&op!='-'&&op!='*'&&op!='/'&&op!='%')break;(*p)++;int b;if(s[*p]=='('){(*p)++;b=calc_eval_expr(s,p);if(s[*p]==')')(*p)++;}else b=calc_num(s,p);if(op=='+')a+=b;else if(op=='-')a-=b;else if(op=='*')a*=b;else if(op=='/'){if(b)a/=b;}else if(op=='%'){if(b)a%=b;}}return a;}

static void calc_run(void){
    term_putcolor("calc",AT(YLW,BLK));term_puts(" - type expressions (e.g. 2+3*4), 'q' to quit\n");
    while(1){
        term_putcolor("calc> ",AT(LGN,BLK));
        char prompt[]="calc> ";
        if(!line_read(prompt,6))continue;
        if(ksc(line_buf,"q")==0||ksc(line_buf,"quit")==0)break;
        if(!line_len)continue;
        int p=0;int r=calc_eval_expr(line_buf,&p);
        term_puts(" = ");term_puti(r);term_nl();
    }
}

/* ── clock ──────────────────────────────── */
static const char*BIG[10][5]={{" ### ","#   #","#   #","#   #"," ### "},{"  #  "," ##  ","  #  ","  #  "," ### "},{" ### ","    #"," ### ","#    ","#####"},{" ### ","    #"," ### ","    #"," ### "},{"#   #","#   #","#####","    #","    #"},{"#####","#    ","#####","    #","#####"},{" ### ","#    ","#####","#   #"," ### "},{"#####","    #","   # ","  #  "," #   "},{" ### ","#   #"," ### ","#   #"," ### "},{" ### ","#   #","#####","    #"," ### "}};
static void clock_run(void){
    term_puts("Clock - press any key to exit\n");
    while(1){
        if(kt!=kh){kgetc();break;}
        uint8_t h,m,s;rtc_ist(&h,&m,&s);
        int dr=5,dc=10;
        int digits[6]={h/10,h%10,m/10,m%10,s/10,s%10};
        for(int d=0;d<6;d++){
            int cx=dc+d*7+(d>=2?2:0)+(d>=4?2:0);
            for(int r=0;r<5;r++)vt(dr+r,cx,BIG[digits[d]][r],AT(LCY,BLK));
        }
        vp(dr+1,dc+14,':',AT(YLW,BLK));vp(dr+3,dc+14,':',AT(YLW,BLK));
        vp(dr+1,dc+30,':',AT(YLW,BLK));vp(dr+3,dc+30,':',AT(YLW,BLK));
        vc_str(12,0,VW,"IST (UTC+5:30)",AT(DGR,BLK));
        msleep(500);
    }
    vcls();term_row=0;term_col=0;
}

/* ── top (system monitor) ───────────────── */
static void top_run(void){
    term_puts("top - press q to quit\n");
    while(1){
        if(kt!=kh){uint8_t k=kgetc();if(k=='q')break;}
        vcls();
        uint8_t h,m,s;rtc_ist(&h,&m,&s);
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,0,"top - Mayura OS v1.0",AT(BLK,CYN));
        char tb[10];tb[0]='0'+h/10;tb[1]='0'+h%10;tb[2]=':';tb[3]='0'+m/10;tb[4]='0'+m%10;tb[5]=':';tb[6]='0'+s/10;tb[7]='0'+s%10;tb[8]=0;
        vt(0,60,tb,AT(BLK,CYN));
        term_row=1;term_col=0;
        term_puts("Tasks: 3 total, 1 running\n");
        char up[32];kss(up,"Uptime: ");ksi(up+ksl(up),ticks/100);kss(up+ksl(up),"s");term_puts(up);term_nl();
        term_puts("CPU:  [");
        int load=(int)(rrand()%30)+5;
        for(int i=0;i<20;i++)term_putc(i<load/5?'|':' ');
        term_puts("]  ");term_puti(load);term_puts("%\n");
        term_puts("MEM:  [====================]  2048MB\n\n");
        term_putcolor("  PID  USER      S  CMD\n",AT(YLW,BLK));
        term_puts("    1  root      R  mayura-kernel\n");
        term_puts("    2  root      S  shell\n");
        term_puts("    3  susanth   R  top\n\n");
        term_puts("Press q to quit");
        msleep(1000);
    }
    vcls();term_row=0;term_col=0;
}

/* ── piano ──────────────────────────────── */
static void spk_on(uint32_t f){uint32_t d=1193180/f;outb(0x43,0xB6);outb(0x42,d&0xFF);outb(0x42,d>>8);outb(0x61,inb(0x61)|3);}
static void spk_off(void){outb(0x61,inb(0x61)&0xFC);}
static const uint32_t NFREQ[]={262,294,330,349,392,440,494,523,587,659,698,784};
static const char NKEYS[]={'a','s','d','f','g','h','j','k','l',';','\'','z'};
static void piano_run(void){
    term_puts("Piano - press a-l keys to play, q to quit\n");
    term_puts("  C  D  E  F  G  A  B  C  D  E  F  G\n");
    term_puts("  a  s  d  f  g  h  j  k  l  ;  '  z\n");
    while(1){
        uint8_t k=kgetc();
        if(k=='q'){spk_off();break;}
        int found=0;
        for(int i=0;i<12;i++)if(k==NKEYS[i]){spk_on(NFREQ[i]);msleep(200);spk_off();found=1;break;}
        if(!found&&k!='q')term_puts("...");
    }
    term_nl();
}

/* ── fortune ────────────────────────────── */
static void cmd_fortune(void){
    /* find quotes file */
    int usr=fs_find("usr",0);int share=fs_find("share",usr);
    int fd=fs_find("fortune",share);int qf=fs_find("quotes",fd);
    if(qf<0){term_puts("No fortune file found.\n");return;}
    /* count lines */
    int nl=0;for(int i=0;i<fs[qf].size;i++)if(fs[qf].data[i]=='\n')nl++;
    if(!nl)return;
    int pick=(int)(rrand()%nl);
    int cur=0;char line[80];int li=0;
    for(int i=0;i<=fs[qf].size;i++){
        if(fs[qf].data[i]=='\n'||i==fs[qf].size){
            if(cur==pick){line[li]=0;term_puts(line);term_nl();return;}
            cur++;li=0;
        }else if(li<79)line[li++]=fs[qf].data[i];
    }
}

/* ── cowsay ─────────────────────────────── */
static void cmd_cowsay(const char*msg){
    int l=ksl(msg);
    term_puts(" ");for(int i=0;i<l+2;i++)term_putc('-');term_nl();
    term_puts("< ");term_puts(msg);term_puts(" >\n");
    term_puts(" ");for(int i=0;i<l+2;i++)term_putc('-');term_nl();
    term_puts("        \\   ^__^\n");
    term_puts("         \\  (oo)\\_______\n");
    term_puts("            (__)\\       )\\/\\\n");
    term_puts("                ||----w |\n");
    term_puts("                ||     ||\n");
}

/* ── neofetch ───────────────────────────── */
static void cmd_neofetch(void){
    uint8_t h,m,s;rtc_ist(&h,&m,&s);
    /* ASCII art logo */
    static const char*logo[]={
        "  .mMMMMMm.    ",
        " mMMMMMMMMMMm  ",
        "MMMMMMMMMMMMMM ",
        "MMM   MMMM MMM ",
        "MMM   MMMM MMM ",
        "MMMMMMMMMMMMMM ",
        " 'MMMMMMMMMM'  ",
        "   'MMMMMM'    ",
        NULL
    };
    static const char*info[]={
        "susanth@mayura",
        "--------------",
        "OS: Mayura OS v1.0",
        "Kernel: i686 bare-metal",
        "Shell: mayura-sh",
        "CPU: Intel x86-32",
        "Memory: 2048MB",
        "IST: --:--:--",
        NULL
    };
    char time_str[10];
    time_str[0]='0'+h/10;time_str[1]='0'+h%10;time_str[2]=':';
    time_str[3]='0'+m/10;time_str[4]='0'+m%10;time_str[5]=':';
    time_str[6]='0'+s/10;time_str[7]='0'+s%10;time_str[8]=0;
    for(int i=0;logo[i]||info[i];i++){
        if(logo[i])term_putcolor(logo[i],AT(LBL,BLK));
        else term_puts("                ");
        term_puts("  ");
        if(info[i]){
            if(i==0)term_putcolor(info[i],AT(YLW,BLK));
            else if(i==1)term_putcolor(info[i],AT(LGR,BLK));
            else if(i==7){term_puts("IST: ");term_puts(time_str);}
            else term_puts(info[i]);
        }
        term_nl();
    }
    /* color blocks */
    term_puts("\n  ");
    for(int i=0;i<8;i++)term_putcolor("  ",AT(i,i));
    term_nl();
    term_puts("  ");
    for(int i=8;i<16;i++)term_putcolor("  ",AT(i,i));
    term_nl();term_nl();
}

/* ── matrix animation ───────────────────── */
static void cmd_matrix(void){
    term_puts("Press any key to stop...\n");msleep(500);
    int col_pos[VW];int col_speed[VW];
    for(int i=0;i<VW;i++){col_pos[i]=(int)(rrand()%VH);col_speed[i]=(int)(rrand()%3)+1;}
    static const char*MCHARS="abcdefghijklmnopqrstuvwxyz0123456789@#$%&";
    int mc=ksl(MCHARS);
    while(kt==kh){
        for(int c=0;c<VW;c++){
            if((int)(rrand()%col_speed[c])==0){
                int r=col_pos[c];
                char ch=MCHARS[(int)(rrand()%mc)];
                vp(r,c,ch,AT(LGN,BLK));
                if(r>0)vp(r-1,c,MCHARS[(int)(rrand()%mc)],AT(DGR,BLK));
                if(r>1)vp(r-2,c,' ',AT(BLK,BLK));
                col_pos[c]=(r+1)%VH;
            }
        }
        msleep(50);
    }
    kgetc();vcls();term_row=0;term_col=0;
}

/* ── fire animation ─────────────────────── */
static void cmd_fire(void){
    term_puts("Press any key to stop...\n");msleep(300);
    static uint8_t fire[VH][VW];
    kms(fire,0,sizeof(fire));
    static const uint8_t FIRE_COLORS[]={BLK,RED,LRD,YLW,WHT};
    while(kt==kh){
        /* seed bottom row */
        for(int c=0;c<VW;c++)
            fire[VH-1][c]=(uint8_t)(rrand()%2?4:3);
        /* propagate */
        for(int r=0;r<VH-1;r++)for(int c=0;c<VW;c++){
            int sum=fire[r+1][(c-1+VW)%VW]+fire[r+1][c]+fire[r+1][(c+1)%VW];
            int v=sum/3;if(v>0&&(int)(rrand()%3)==0)v--;
            fire[r][c]=(uint8_t)v;
            vp(r,c,fire[r][c]>0?'\xDB':' ',AT(FIRE_COLORS[fire[r][c]<5?fire[r][c]:4],BLK));
        }
        msleep(60);
    }
    kgetc();vcls();term_row=0;term_col=0;
}

/* ── starfield ──────────────────────────── */
static void cmd_starfield(void){
    term_puts("Press any key to stop...\n");msleep(300);
    typedef struct{int x,y,z;}Star;
    Star stars[80];
    for(int i=0;i<80;i++){stars[i].x=(int)(rrand()%160)-80;stars[i].y=(int)(rrand()%50)-25;stars[i].z=(int)(rrand()%32)+1;}
    while(kt==kh){
        vcls_color(AT(BLK,BLK));
        for(int i=0;i<80;i++){
            stars[i].z--;if(stars[i].z<=0){stars[i].x=(int)(rrand()%160)-80;stars[i].y=(int)(rrand()%50)-25;stars[i].z=32;}
            int sx=40+stars[i].x*16/stars[i].z;
            int sy=12+stars[i].y*12/stars[i].z;
            if(sx>=0&&sx<VW&&sy>=0&&sy<VH){
                uint8_t bright=stars[i].z<8?WHT:stars[i].z<16?LGR:DGR;
                char ch=stars[i].z<8?'\xDB':stars[i].z<16?'.':'`';
                vp(sy,sx,ch,AT(bright,BLK));
            }
        }
        msleep(40);
    }
    kgetc();vcls();term_row=0;term_col=0;
}

/* ── sl (steam locomotive) ──────────────── */
static void cmd_sl(void){
    static const char*SL[]={
        "      ====        ________                ___________    ",
        "  _D _|  |_______/        \\__I_I_____===__|___________|  ",
        "   |(_)---  |   H\\________/ |   |        =|___ ___|     ",
        "   /     |  |   H  |  |     |   |         ||_| |_||     ",
        "  |      |  |   H  |__--------------------| [___] |     ",
        "  | ________|___H__/__|_____/[][]~\\_______|       |     ",
        "  |/ |   |-----------I_____I [][] []  D   |=====__| \\   ",
        "__/ =| o |=-~~\\  /~~\\  /~~\\  /~~\\ ____Y___________|__   ",
        " |/-=|___|=    ||    ||    ||    |_____/~\\___/        \\  ",
        "  \\_/      \\O=====O=====O=====O_/      \\_/           \\/ "
    };
    int offset=VW+60;
    while(offset>-60){
        vcls_color(AT(BLK,BLK));
        for(int r=0;r<10;r++){
            int c=offset;
            const char*s=SL[r];
            for(int j=0;s[j]&&c<VW;j++,c++)if(c>=0)vp(r+5,c,s[j],AT(LGN,BLK));
        }
        offset-=2;msleep(60);
        if(kt!=kh){kgetc();break;}
    }
    vcls();term_row=0;term_col=0;
}

/* ── hack animation ─────────────────────── */
static void cmd_hack(void){
    const char*lines[]={
        "Initializing exploit framework...",
        "Scanning target: 192.168.1.1",
        "Port 22 open: SSH detected",
        "Loading payload modules...",
        "Bypassing firewall [=====>    ] 60%",
        "Bypassing firewall [=========] 100%",
        "Injecting shellcode...",
        "Privilege escalation: ROOT ACQUIRED",
        "Downloading /etc/shadow...",
        "Exfiltrating 2.4GB of data...",
        "Covering tracks...",
        "Wiping logs...",
        "Connection closed. Mission complete.",
        NULL
    };
    term_putcolor("[MAYURA HACKER SIMULATOR]\n\n",AT(LGN,BLK));
    for(int i=0;lines[i];i++){
        term_putcolor(lines[i],AT(LGN,BLK));
        term_nl();
        msleep(400+((int)(rrand()%3))*200);
    }
    term_nl();
    term_putcolor("[NOTE: This is a simulation. No actual hacking occurred.]\n",AT(YLW,BLK));
}

/* ── figlet (big ASCII text) ─────────────── */
static void cmd_figlet(const char*msg){
    int l=ksl(msg);
    /* just triple-width each char since we have no font data */
    for(int row=0;row<3;row++){
        for(int i=0;i<l;i++){
            char c=msg[i];
            if(c>='a'&&c<='z')c=c-32;
            if(row==0){term_putc(' ');term_putc(c);term_putc(c);term_putc(' ');}
            else if(row==1){term_putc(c);term_putc(' ');term_putc(' ');term_putc(c);}
            else{term_putc(' ');term_putc(c);term_putc(c);term_putc(' ');}
        }
        term_nl();
    }
}

/* ── weather (fake) ─────────────────────── */
static void cmd_weather(void){
    static const char*conditions[]={"Sunny","Partly Cloudy","Cloudy","Light Rain","Thunderstorm","Foggy"};
    int cond=(int)(rrand()%6);
    int temp=20+(int)(rrand()%20);
    int humid=40+(int)(rrand()%50);
    term_putcolor("Mayura Weather Service\n",AT(YLW,BLK));
    term_puts("-----------------------\n");
    term_puts("Location: Bengaluru, IN\n");
    term_puts("Condition: ");term_puts(conditions[cond]);term_nl();
    term_puts("Temperature: ");term_puti(temp);term_puts(" C\n");
    term_puts("Humidity: ");term_puti(humid);term_puts("%\n");
    term_puts("Wind: ");term_puti((int)(rrand()%30));term_puts(" km/h\n");
    term_putcolor("[Simulated data]\n",AT(DGR,BLK));
}

/* ── about / credits ────────────────────── */
static void cmd_about(void){
    term_putcolor("\n  Mayura OS v1.0\n",AT(YLW,BLK));
    term_putcolor("  ══════════════\n",AT(LGR,BLK));
    term_puts("  A bare-metal x86 operating system\n\n");
    term_putcolor("  Created by: ",AT(LGR,BLK));
    term_putcolor("Susanth\n",AT(LGN,BLK));
    term_puts("  Architecture: i686 x86-32\n");
    term_puts("  Boot: GRUB Multiboot v1\n");
    term_puts("  Video: VGA Text Mode 80x25\n");
    term_puts("  Language: SimpleC built-in\n");
    term_puts("  No libc. No OS beneath.\n");
    term_puts("  Pure silicon. Pure soul.\n\n");
}

/* ── om (secret) ────────────────────────── */
static void cmd_om(void){
    vcls();
    /* Center screen display */
    int r=4;
    /* Om symbol approximation with box chars */
    vf(r,  0,VW,' ',AT(BLK,BLK));
    vf(r+1,0,VW,' ',AT(BLK,BLK));
    /* Big OM text */
    vc_str(r,  0,VW,"   \xE9\xA6\x96   ",AT(YLW,BLK));/* best CP437 approx */
    vc_str(r+1,0,VW,"  \xDB\xDB\xDB \xDB\xDB\xDB  ",AT(YLW,BLK));
    vc_str(r+2,0,VW," \xDB   \xDB\xDB\xDB\xDB  ",AT(YLW,BLK));
    vc_str(r+3,0,VW,"  \xDB\xDB  \xDB    ",AT(YLW,BLK));
    vc_str(r+4,0,VW,"     \xDB\xDB\xDB\xDB  ",AT(YLW,BLK));
    /* Om text */
    vc_str(r+1,0,VW,"         \xDB\xDB\xDB",AT(YLW,BLK));
    /* Draw proper OM using VGA chars */
    vt(r  ,28,"  \xDB\xDB\xDB  \xDB\xDB ",AT(YLW,BLK));
    vt(r+1,28," \xDB   \xDB\xDB\xDB  ",AT(YLW,BLK));
    vt(r+2,28,"  \xDB\xDB  \xDB   ",AT(YLW,BLK));
    vt(r+3,28,"     \xDB\xDB\xDB  ",AT(YLW,BLK));
    vt(r+4,28,"       \xDB   ",AT(YLW,BLK));
    vt(r+5,28,"  \xDB\xDB\xDB\xDB\xDB  ",AT(YLW,BLK));
    /* Title */
    vc_str(r+7,0,VW,"~ Om ~",AT(YLW,BLK));
    /* Venkatesha symbol */
    vc_str(r+10,0,VW,"\xDB\xDB  Venkatesha  \xDB\xDB",AT(LGN,BLK));
    vc_str(r+12,0,VW,"Om Namo Venkatesaya",AT(PNK,BLK));
    vc_str(r+14,0,VW,"Created by Susanth",AT(LCY,BLK));
    vc_str(r+16,0,VW,"~ Mayura OS v1.0 ~",AT(DGR,BLK));
    vc_str(r+18,0,VW,"Press any key...",AT(DGR,BLK));
    kgetc();
    vcls();term_row=0;term_col=0;
}

/* ── konami ─────────────────────────────── */
static void cmd_konami(void){
    term_puts("Enter Konami code: UP UP DN DN LT RT LT RT B A\n");
    term_puts("(use arrow keys then b and a)\n");
    const uint8_t seq[]={K_UP,K_UP,K_DN,K_DN,K_LT,K_RT,K_LT,K_RT,'b','a'};
    int pos=0;
    uint32_t timeout=ticks+500;
    while(ticks<timeout&&pos<10){
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k==seq[pos])pos++;
            else pos=0;
            timeout=ticks+500;
        }
        __asm__("hlt");
    }
    if(pos==10){
        term_putcolor("\n  KONAMI CODE ACTIVATED!\n",AT(YLW,BLK));
        term_putcolor("  +30 lives granted to Susanth\n",AT(LGN,BLK));
        term_putcolor("  Achievement unlocked: OG Gamer\n\n",AT(PNK,BLK));
    }else{
        term_puts("Timed out. Try again.\n");
    }
}

/* ════════════════════════════════════════════
   SIMPLEC INTERPRETER
════════════════════════════════════════════ */
#define SC_MAXVARS   64
#define SC_STRMAX    128
#define SC_FUNCMAX   16
#define SC_TOKMAX    512
#define SC_CODEMAX   4096
#define SC_CALLMAX   8

typedef enum{
    SC_INT,SC_STR
}SCType;

typedef struct{
    char name[32];
    SCType type;
    int ival;
    char sval[SC_STRMAX];
}SCVar;

typedef enum{
    TOK_INT,TOK_STR,TOK_IDENT,
    TOK_PLUS,TOK_MINUS,TOK_STAR,TOK_SLASH,TOK_PERC,
    TOK_EQ,TOK_NEQ,TOK_LT,TOK_GT,TOK_LE,TOK_GE,
    TOK_AND,TOK_OR,TOK_NOT,
    TOK_ASSIGN,TOK_SEMI,TOK_COMMA,
    TOK_LPAREN,TOK_RPAREN,TOK_LBRACE,TOK_RBRACE,
    TOK_IF,TOK_ELSE,TOK_WHILE,TOK_FOR,TOK_RETURN,TOK_BREAK,
    TOK_INT_KW,TOK_STR_KW,TOK_BOOL_KW,
    TOK_PRINTLN,TOK_PRINT,TOK_INPUT,TOK_CLEAR,TOK_SLEEP,TOK_RAND,
    /* Math lib */
    TOK_ABS,TOK_MIN,TOK_MAX,TOK_POW,TOK_SQRT,TOK_MOD,
    /* String lib */
    TOK_LEN,TOK_UPPER,TOK_LOWER,TOK_CONCAT,TOK_SUBSTR,TOK_STRCMP,TOK_CONTAINS,
    /* IO lib */
    TOK_READFILE,TOK_WRITEFILE,TOK_APPENDFILE,
    /* System lib */
    TOK_TIME,TOK_TICKS,TOK_BEEP,TOK_EXIT,
    /* Array */
    TOK_ARRAY,
    TOK_EOF,TOK_ERR
}SCTok;

typedef struct{
    SCTok type;
    int ival;
    char sval[SC_STRMAX];
}SCToken;

static SCVar sc_vars[SC_MAXVARS];
static int sc_nvar=0;
static char sc_code[SC_CODEMAX];
static int sc_code_len=0;
static SCToken sc_toks[SC_TOKMAX];
static int sc_ntok=0;
static int sc_tpos=0;
static int sc_error=0;
static char sc_errmsg[80];
static int sc_ret_val=0;
static int sc_breaking=0;

static SCVar*sc_getvar(const char*name){for(int i=0;i<sc_nvar;i++)if(ksc(sc_vars[i].name,name)==0)return &sc_vars[i];return 0;}
static SCVar*sc_setvar(const char*name,SCType t){SCVar*v=sc_getvar(name);if(!v){if(sc_nvar>=SC_MAXVARS)return 0;v=&sc_vars[sc_nvar++];kss(v->name,name);}v->type=t;return v;}

/* tokenizer */
static void sc_tokenize(void){
    sc_ntok=0;sc_error=0;
    int i=0;
    while(i<sc_code_len&&sc_ntok<SC_TOKMAX-1){
        char c=sc_code[i];
        /* skip whitespace */
        if(c==' '||c=='\t'||c=='\n'||c=='\r'){i++;continue;}
        /* skip comments */
        if(c=='/'&&sc_code[i+1]=='/'){while(i<sc_code_len&&sc_code[i]!='\n')i++;continue;}
        /* string literal */
        if(c=='"'){
            i++;int j=0;
            while(i<sc_code_len&&sc_code[i]!='"'&&j<SC_STRMAX-1){
                if(sc_code[i]=='\\'&&sc_code[i+1]=='n'){sc_toks[sc_ntok].sval[j++]='\n';i+=2;}
                else sc_toks[sc_ntok].sval[j++]=sc_code[i++];
            }
            sc_toks[sc_ntok].sval[j]=0;sc_toks[sc_ntok].type=TOK_STR;sc_ntok++;
            if(sc_code[i]=='"')i++;
            continue;
        }
        /* number */
        if(c>='0'&&c<='9'){
            int n=0;while(i<sc_code_len&&sc_code[i]>='0'&&sc_code[i]<='9')n=n*10+(sc_code[i++]-'0');
            sc_toks[sc_ntok].ival=n;sc_toks[sc_ntok].type=TOK_INT;sc_ntok++;continue;
        }
        /* identifier or keyword */
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'){
            int j=0;
            while(i<sc_code_len&&((sc_code[i]>='a'&&sc_code[i]<='z')||(sc_code[i]>='A'&&sc_code[i]<='Z')||(sc_code[i]>='0'&&sc_code[i]<='9')||sc_code[i]=='_')&&j<31)
                sc_toks[sc_ntok].sval[j++]=sc_code[i++];
            sc_toks[sc_ntok].sval[j]=0;
            const char*id=sc_toks[sc_ntok].sval;
            if(ksc(id,"if")==0)sc_toks[sc_ntok].type=TOK_IF;
            else if(ksc(id,"else")==0)sc_toks[sc_ntok].type=TOK_ELSE;
            else if(ksc(id,"while")==0)sc_toks[sc_ntok].type=TOK_WHILE;
            else if(ksc(id,"for")==0)sc_toks[sc_ntok].type=TOK_FOR;
            else if(ksc(id,"return")==0)sc_toks[sc_ntok].type=TOK_RETURN;
            else if(ksc(id,"break")==0)sc_toks[sc_ntok].type=TOK_BREAK;
            else if(ksc(id,"int")==0)sc_toks[sc_ntok].type=TOK_INT_KW;
            else if(ksc(id,"string")==0)sc_toks[sc_ntok].type=TOK_STR_KW;
            else if(ksc(id,"bool")==0)sc_toks[sc_ntok].type=TOK_BOOL_KW;
            else if(ksc(id,"println")==0)sc_toks[sc_ntok].type=TOK_PRINTLN;
            else if(ksc(id,"print")==0)sc_toks[sc_ntok].type=TOK_PRINT;
            else if(ksc(id,"input")==0)sc_toks[sc_ntok].type=TOK_INPUT;
            else if(ksc(id,"clear")==0)sc_toks[sc_ntok].type=TOK_CLEAR;
            else if(ksc(id,"sleep")==0)sc_toks[sc_ntok].type=TOK_SLEEP;
            else if(ksc(id,"rand")==0)sc_toks[sc_ntok].type=TOK_RAND;
            else if(ksc(id,"true")==0){sc_toks[sc_ntok].type=TOK_INT;sc_toks[sc_ntok].ival=1;}
            else if(ksc(id,"false")==0){sc_toks[sc_ntok].type=TOK_INT;sc_toks[sc_ntok].ival=0;}
            /* math lib */
            else if(ksc(id,"abs")==0)sc_toks[sc_ntok].type=TOK_ABS;
            else if(ksc(id,"min")==0)sc_toks[sc_ntok].type=TOK_MIN;
            else if(ksc(id,"max")==0)sc_toks[sc_ntok].type=TOK_MAX;
            else if(ksc(id,"pow")==0)sc_toks[sc_ntok].type=TOK_POW;
            else if(ksc(id,"sqrt")==0)sc_toks[sc_ntok].type=TOK_SQRT;
            else if(ksc(id,"mod")==0)sc_toks[sc_ntok].type=TOK_MOD;
            /* string lib */
            else if(ksc(id,"len")==0)sc_toks[sc_ntok].type=TOK_LEN;
            else if(ksc(id,"upper")==0)sc_toks[sc_ntok].type=TOK_UPPER;
            else if(ksc(id,"lower")==0)sc_toks[sc_ntok].type=TOK_LOWER;
            else if(ksc(id,"concat")==0)sc_toks[sc_ntok].type=TOK_CONCAT;
            else if(ksc(id,"substr")==0)sc_toks[sc_ntok].type=TOK_SUBSTR;
            else if(ksc(id,"strcmp")==0)sc_toks[sc_ntok].type=TOK_STRCMP;
            else if(ksc(id,"contains")==0)sc_toks[sc_ntok].type=TOK_CONTAINS;
            /* io lib */
            else if(ksc(id,"readfile")==0)sc_toks[sc_ntok].type=TOK_READFILE;
            else if(ksc(id,"writefile")==0)sc_toks[sc_ntok].type=TOK_WRITEFILE;
            else if(ksc(id,"appendfile")==0)sc_toks[sc_ntok].type=TOK_APPENDFILE;
            /* sys lib */
            else if(ksc(id,"gettime")==0)sc_toks[sc_ntok].type=TOK_TIME;
            else if(ksc(id,"getticks")==0)sc_toks[sc_ntok].type=TOK_TICKS;
            else if(ksc(id,"beep")==0)sc_toks[sc_ntok].type=TOK_BEEP;
            else if(ksc(id,"exit")==0)sc_toks[sc_ntok].type=TOK_EXIT;
            else sc_toks[sc_ntok].type=TOK_IDENT;
            sc_ntok++;continue;
        }
        /* operators */
        SCTok t=TOK_ERR;
        if(c=='+'&&sc_code[i+1]!='=')t=TOK_PLUS;
        else if(c=='-'&&sc_code[i+1]!='=')t=TOK_MINUS;
        else if(c=='*')t=TOK_STAR;
        else if(c=='/')t=TOK_SLASH;
        else if(c=='%')t=TOK_PERC;
        else if(c=='='&&sc_code[i+1]=='='){t=TOK_EQ;i++;}
        else if(c=='!'&&sc_code[i+1]=='='){t=TOK_NEQ;i++;}
        else if(c=='<'&&sc_code[i+1]=='='){t=TOK_LE;i++;}
        else if(c=='>'&&sc_code[i+1]=='='){t=TOK_GE;i++;}
        else if(c=='<')t=TOK_LT;
        else if(c=='>')t=TOK_GT;
        else if(c=='&'&&sc_code[i+1]=='&'){t=TOK_AND;i++;}
        else if(c=='|'&&sc_code[i+1]=='|'){t=TOK_OR;i++;}
        else if(c=='!')t=TOK_NOT;
        else if(c=='=')t=TOK_ASSIGN;
        else if(c==';')t=TOK_SEMI;
        else if(c==',')t=TOK_COMMA;
        else if(c=='(')t=TOK_LPAREN;
        else if(c==')')t=TOK_RPAREN;
        else if(c=='{')t=TOK_LBRACE;
        else if(c=='}')t=TOK_RBRACE;
        if(t!=TOK_ERR){sc_toks[sc_ntok].type=t;sc_ntok++;i++;}
        else i++;/* skip unknown */
    }
    sc_toks[sc_ntok].type=TOK_EOF;sc_ntok++;
}

/* forward declarations */
static int sc_expr(void);
static void sc_stmt(void);
static void sc_block(void);

static SCToken sc_peek(void){return sc_tpos<sc_ntok?sc_toks[sc_tpos]:sc_toks[sc_ntok-1];}
static SCToken sc_next(void){return sc_tpos<sc_ntok?sc_toks[sc_tpos++]:sc_toks[sc_ntok-1];}
static int sc_expect(SCTok t){if(sc_peek().type==t){sc_next();return 1;}sc_error=1;kss(sc_errmsg,"Unexpected token");return 0;}

/* expression evaluator */
static char sc_strbuf[SC_STRMAX];
static void sc_str_expr(void){
    sc_strbuf[0]=0;
    SCToken t=sc_peek();
    if(t.type==TOK_STR){sc_next();kss(sc_strbuf,t.sval);return;}
    if(t.type==TOK_IDENT){
        sc_next();SCVar*v=sc_getvar(t.sval);
        if(v){if(v->type==SC_STR)kss(sc_strbuf,v->sval);else ksi(sc_strbuf,v->ival);}
        return;
    }
    /* fallback: evaluate as int and convert */
    int n=sc_expr();ksi(sc_strbuf,n);
}

static int sc_isqrt(int n){if(n<=0)return 0;int x=n,y=(x+1)/2;while(y<x){x=y;y=(x+n/x)/2;}return x;}
static int sc_ipow(int b,int e){int r=1;if(e<0)return 0;while(e-->0)r*=b;return r;}

static int sc_primary(void){
    SCToken t=sc_peek();
    if(t.type==TOK_INT){sc_next();return t.ival;}
    if(t.type==TOK_IDENT){
        sc_next();
        if(sc_peek().type==TOK_LPAREN){
            /* function call - not supported, skip args */
            sc_next();int depth=1;
            while(depth>0&&sc_peek().type!=TOK_EOF){
                if(sc_peek().type==TOK_LPAREN)depth++;
                else if(sc_peek().type==TOK_RPAREN)depth--;
                sc_next();
            }
            return 0;
        }
        SCVar*v=sc_getvar(t.sval);
        if(v)return v->ival;
        return 0;
    }
    if(t.type==TOK_MINUS){sc_next();return -sc_primary();}
    if(t.type==TOK_NOT){sc_next();return !sc_primary();}
    if(t.type==TOK_LPAREN){
        sc_next();int v=sc_expr();sc_expect(TOK_RPAREN);return v;
    }
    if(t.type==TOK_RAND){
        sc_next();sc_expect(TOK_LPAREN);int max=sc_expr();sc_expect(TOK_RPAREN);
        return (int)(rrand()%(max>0?max:1));
    }
    /* math lib */
    if(t.type==TOK_ABS){sc_next();sc_expect(TOK_LPAREN);int v=sc_expr();sc_expect(TOK_RPAREN);return v<0?-v:v;}
    if(t.type==TOK_SQRT){sc_next();sc_expect(TOK_LPAREN);int v=sc_expr();sc_expect(TOK_RPAREN);return sc_isqrt(v);}
    if(t.type==TOK_POW){sc_next();sc_expect(TOK_LPAREN);int b=sc_expr();sc_expect(TOK_COMMA);int e=sc_expr();sc_expect(TOK_RPAREN);return sc_ipow(b,e);}
    if(t.type==TOK_MIN){sc_next();sc_expect(TOK_LPAREN);int a=sc_expr();sc_expect(TOK_COMMA);int b=sc_expr();sc_expect(TOK_RPAREN);return a<b?a:b;}
    if(t.type==TOK_MAX){sc_next();sc_expect(TOK_LPAREN);int a=sc_expr();sc_expect(TOK_COMMA);int b=sc_expr();sc_expect(TOK_RPAREN);return a>b?a:b;}
    if(t.type==TOK_MOD){sc_next();sc_expect(TOK_LPAREN);int a=sc_expr();sc_expect(TOK_COMMA);int b=sc_expr();sc_expect(TOK_RPAREN);return b?a%b:0;}
    /* string lib - return lengths/comparison as int */
    if(t.type==TOK_LEN){
        sc_next();sc_expect(TOK_LPAREN);SCToken arg=sc_peek();int ln=0;
        if(arg.type==TOK_STR){sc_next();ln=ksl(arg.sval);}
        else if(arg.type==TOK_IDENT){sc_next();SCVar*v=sc_getvar(arg.sval);if(v){ln=(v->type==SC_STR)?ksl(v->sval):0;}}
        sc_expect(TOK_RPAREN);return ln;
    }
    if(t.type==TOK_STRCMP){
        sc_next();sc_expect(TOK_LPAREN);
        sc_str_expr();char a2[SC_STRMAX];kss(a2,sc_strbuf);
        sc_expect(TOK_COMMA);sc_str_expr();
        sc_expect(TOK_RPAREN);return ksc(a2,sc_strbuf);
    }
    if(t.type==TOK_CONTAINS){
        sc_next();sc_expect(TOK_LPAREN);
        sc_str_expr();char haystack[SC_STRMAX];kss(haystack,sc_strbuf);
        sc_expect(TOK_COMMA);sc_str_expr();
        sc_expect(TOK_RPAREN);
        int pl=ksl(sc_strbuf);
        for(int ki=0;haystack[ki];ki++)if(knc(haystack+ki,sc_strbuf,pl)==0)return 1;
        return 0;
    }
    /* sys lib */
    if(t.type==TOK_TICKS){sc_next();sc_expect(TOK_LPAREN);sc_expect(TOK_RPAREN);return (int)ticks;}
    if(t.type==TOK_TIME){
        sc_next();sc_expect(TOK_LPAREN);sc_expect(TOK_RPAREN);
        uint8_t hh2,mm2,ss2;rtc_ist(&hh2,&mm2,&ss2);
        return hh2*3600+mm2*60+ss2;
    }
    sc_next();return 0;
}
static int sc_mul(void){int a=sc_primary();while(!sc_error&&(sc_peek().type==TOK_STAR||sc_peek().type==TOK_SLASH||sc_peek().type==TOK_PERC)){SCToken op=sc_next();int b=sc_primary();if(op.type==TOK_STAR)a*=b;else if(op.type==TOK_SLASH){if(b)a/=b;}else if(b)a%=b;}return a;}
static int sc_add(void){int a=sc_mul();while(!sc_error&&(sc_peek().type==TOK_PLUS||sc_peek().type==TOK_MINUS)){SCToken op=sc_next();int b=sc_mul();a=(op.type==TOK_PLUS)?a+b:a-b;}return a;}
static int sc_cmp(void){int a=sc_add();while(!sc_error&&(sc_peek().type==TOK_LT||sc_peek().type==TOK_GT||sc_peek().type==TOK_LE||sc_peek().type==TOK_GE||sc_peek().type==TOK_EQ||sc_peek().type==TOK_NEQ)){SCToken op=sc_next();int b=sc_add();if(op.type==TOK_LT)a=a<b;else if(op.type==TOK_GT)a=a>b;else if(op.type==TOK_LE)a=a<=b;else if(op.type==TOK_GE)a=a>=b;else if(op.type==TOK_EQ)a=a==b;else a=a!=b;}return a;}
static int sc_expr(void){int a=sc_cmp();while(!sc_error&&(sc_peek().type==TOK_AND||sc_peek().type==TOK_OR)){SCToken op=sc_next();int b=sc_cmp();a=(op.type==TOK_AND)?a&&b:a||b;}return a;}

/* string expression - returns in static buffer */
/* statement executor */
static void sc_block(void){
    sc_expect(TOK_LBRACE);
    while(!sc_error&&sc_peek().type!=TOK_RBRACE&&sc_peek().type!=TOK_EOF)
        sc_stmt();
    sc_expect(TOK_RBRACE);
}

static void sc_stmt(void){
    if(sc_error||sc_breaking)return;
    SCToken t=sc_peek();

    /* variable declaration: int x = ...; or string s = ...; */
    if(t.type==TOK_INT_KW||t.type==TOK_STR_KW||t.type==TOK_BOOL_KW){
        SCTok kw=sc_next().type;
        char vname[32];kss(vname,sc_next().sval);
        if(sc_peek().type==TOK_ASSIGN){
            sc_next();
            if(kw==TOK_STR_KW){
                sc_str_expr();
                SCVar*v=sc_setvar(vname,SC_STR);if(v)kss(v->sval,sc_strbuf);
            }else{
                int val=sc_expr();
                SCVar*v=sc_setvar(vname,SC_INT);if(v)v->ival=val;
            }
        }else{
            SCVar*v=sc_setvar(vname,kw==TOK_STR_KW?SC_STR:SC_INT);
            if(v){v->ival=0;v->sval[0]=0;}
        }
        sc_expect(TOK_SEMI);return;
    }

    /* assignment: x = ...; */
    if(t.type==TOK_IDENT&&sc_tpos+1<sc_ntok&&sc_toks[sc_tpos+1].type==TOK_ASSIGN){
        char vname[32];kss(vname,sc_next().sval);sc_next();/* = */
        SCVar*v=sc_getvar(vname);
        if(v&&v->type==SC_STR){sc_str_expr();kss(v->sval,sc_strbuf);}
        else{int val=sc_expr();if(v)v->ival=val;else{v=sc_setvar(vname,SC_INT);if(v)v->ival=val;}}
        sc_expect(TOK_SEMI);return;
    }

    /* println / print */
    if(t.type==TOK_PRINTLN||t.type==TOK_PRINT){
        int nl=(t.type==TOK_PRINTLN);sc_next();sc_expect(TOK_LPAREN);
        SCToken arg=sc_peek();
        if(arg.type==TOK_STR){sc_next();term_puts(arg.sval);}
        else{
            /* check if ident is string */
            if(arg.type==TOK_IDENT){
                SCVar*v=sc_getvar(arg.sval);
                if(v&&v->type==SC_STR){sc_next();term_puts(v->sval);}
                else{int n=sc_expr();term_puti(n);}
            }else{int n=sc_expr();term_puti(n);}
        }
        if(nl)term_nl();
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);return;
    }

    /* input */
    if(t.type==TOK_INPUT){
        sc_next();sc_expect(TOK_LPAREN);
        char vname[32];kss(vname,sc_next().sval);sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        term_putcolor("> ",AT(LGN,BLK));
        if(!line_read("> ",2))return;
        SCVar*v=sc_getvar(vname);
        if(!v){v=sc_setvar(vname,SC_STR);}
        if(v){
            /* try to parse as int */
            int allnum=1;for(int i=0;i<line_len;i++)if(!(line_buf[i]>='0'&&line_buf[i]<='9')&&line_buf[i]!='-')allnum=0;
            if(allnum&&line_len){v->type=SC_INT;v->ival=katoi(line_buf);}
            else{v->type=SC_STR;kss(v->sval,line_buf);}
        }
        return;
    }

    /* clear */
    if(t.type==TOK_CLEAR){sc_next();sc_expect(TOK_LPAREN);sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);term_clear();return;}

    /* sleep */
    if(t.type==TOK_SLEEP){sc_next();sc_expect(TOK_LPAREN);int ms=sc_expr();sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);msleep(ms);return;}
    /* beep */
    if(t.type==TOK_BEEP){
        sc_next();sc_expect(TOK_LPAREN);
        int freq2=sc_expr();int dur2=200;
        if(sc_peek().type==TOK_COMMA){sc_next();dur2=sc_expr();}
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        spk_on(freq2);msleep(dur2);spk_off();return;
    }
    /* exit */
    if(t.type==TOK_EXIT){sc_next();sc_expect(TOK_LPAREN);sc_expr();sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);sc_breaking=1;return;}
    /* upper/lower for strings */
    if(t.type==TOK_UPPER||t.type==TOK_LOWER){
        int isup=(t.type==TOK_UPPER);sc_next();sc_expect(TOK_LPAREN);
        char vname2[32];kss(vname2,sc_next().sval);sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        SCVar*v=sc_getvar(vname2);
        if(v&&v->type==SC_STR){if(isup)kupper(v->sval);else klower(v->sval);}
        return;
    }
    /* concat */
    if(t.type==TOK_CONCAT){
        sc_next();sc_expect(TOK_LPAREN);
        char vname2[32];kss(vname2,sc_next().sval);
        sc_expect(TOK_COMMA);sc_str_expr();
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        SCVar*v=sc_getvar(vname2);
        if(v&&v->type==SC_STR){int cl=ksl(v->sval);int al=ksl(sc_strbuf);if(cl+al<SC_STRMAX-1){kmc(v->sval+cl,sc_strbuf,al+1);}}
        return;
    }
    /* substr */
    if(t.type==TOK_SUBSTR){
        sc_next();sc_expect(TOK_LPAREN);
        char vname2[32];kss(vname2,sc_next().sval);
        sc_expect(TOK_COMMA);int start=sc_expr();
        sc_expect(TOK_COMMA);int slen2=sc_expr();
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        SCVar*v=sc_getvar(vname2);
        if(v&&v->type==SC_STR){
            char tmp[SC_STRMAX];int j=0;
            int sl=ksl(v->sval);
            if(start<0)start=0;if(start>sl)start=sl;
            for(int si=start;si<sl&&j<slen2&&j<SC_STRMAX-1;si++)tmp[j++]=v->sval[si];
            tmp[j]=0;kss(v->sval,tmp);
        }
        return;
    }
    /* readfile */
    if(t.type==TOK_READFILE){
        sc_next();sc_expect(TOK_LPAREN);
        char vname2[32];kss(vname2,sc_next().sval);
        sc_expect(TOK_COMMA);sc_str_expr();/* filename */
        char fname2[32];kss(fname2,sc_strbuf);
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        int fn=fs_find(fname2,fs_cwd);
        SCVar*v=sc_getvar(vname2);if(!v)v=sc_setvar(vname2,SC_STR);
        if(v){v->type=SC_STR;if(fn>=0){int cl=fs[fn].size<SC_STRMAX-1?fs[fn].size:SC_STRMAX-2;kmc(v->sval,fs[fn].data,cl);v->sval[cl]=0;}else kss(v->sval,"");}
        return;
    }
    /* writefile */
    if(t.type==TOK_WRITEFILE){
        sc_next();sc_expect(TOK_LPAREN);
        sc_str_expr();char fname2[32];kss(fname2,sc_strbuf);
        sc_expect(TOK_COMMA);sc_str_expr();
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        int fn=fs_find(fname2,fs_cwd);
        if(fn<0){fn=fs_alloc();if(fn>=0){fs[fn].used=1;fs[fn].is_dir=0;kss(fs[fn].name,fname2);fs[fn].parent=fs_cwd;}}
        if(fn>=0){int cl=ksl(sc_strbuf);if(cl>=FS_DSIZ-1)cl=FS_DSIZ-2;kmc(fs[fn].data,sc_strbuf,cl);fs[fn].data[cl]=0;fs[fn].size=cl;}
        return;
    }
    /* appendfile */
    if(t.type==TOK_APPENDFILE){
        sc_next();sc_expect(TOK_LPAREN);
        sc_str_expr();char fname2[32];kss(fname2,sc_strbuf);
        sc_expect(TOK_COMMA);sc_str_expr();
        sc_expect(TOK_RPAREN);sc_expect(TOK_SEMI);
        int fn=fs_find(fname2,fs_cwd);
        if(fn>=0){int cl=ksl(sc_strbuf);int av=FS_DSIZ-1-fs[fn].size;if(cl>av)cl=av;kmc(fs[fn].data+fs[fn].size,sc_strbuf,cl);fs[fn].size+=cl;}
        return;
    }

    /* if */
    if(t.type==TOK_IF){
        sc_next();sc_expect(TOK_LPAREN);int cond=sc_expr();sc_expect(TOK_RPAREN);
        if(cond)sc_block();else{/* skip block */int depth=0;sc_expect(TOK_LBRACE);depth=1;while(depth>0&&sc_peek().type!=TOK_EOF){if(sc_peek().type==TOK_LBRACE)depth++;else if(sc_peek().type==TOK_RBRACE)depth--;sc_next();}}
        if(sc_peek().type==TOK_ELSE){
            sc_next();
            if(!cond)sc_block();else{int depth=0;sc_expect(TOK_LBRACE);depth=1;while(depth>0&&sc_peek().type!=TOK_EOF){if(sc_peek().type==TOK_LBRACE)depth++;else if(sc_peek().type==TOK_RBRACE)depth--;sc_next();}}
        }
        return;
    }

    /* while */
    if(t.type==TOK_WHILE){
        sc_next();int cond_start=sc_tpos;
        sc_expect(TOK_LPAREN);int cond=sc_expr();sc_expect(TOK_RPAREN);
        int body_start=sc_tpos;
        /* find end of block to know where to loop back */
        if(!cond){int depth=0;sc_expect(TOK_LBRACE);depth=1;while(depth>0&&sc_peek().type!=TOK_EOF){if(sc_peek().type==TOK_LBRACE)depth++;else if(sc_peek().type==TOK_RBRACE)depth--;sc_next();}return;}
        int iters=0;
        while(cond&&!sc_error&&!sc_breaking&&iters<10000){
            sc_tpos=body_start;sc_block();
            sc_tpos=cond_start;sc_expect(TOK_LPAREN);cond=sc_expr();sc_expect(TOK_RPAREN);
            iters++;
        }
        sc_breaking=0;
        if(iters>=10000){term_putcolor("Error: infinite loop detected\n",AT(LRD,BLK));}
        return;
    }

    /* for */
    if(t.type==TOK_FOR){
        sc_next();sc_expect(TOK_LPAREN);
        sc_stmt();/* init */
        int cond_start=sc_tpos;
        int cond=sc_expr();sc_expect(TOK_SEMI);
        int inc_start=sc_tpos;
        /* skip increment to find body */
        int depth2=0;while(sc_peek().type!=TOK_RPAREN||depth2>0){if(sc_peek().type==TOK_LPAREN)depth2++;else if(sc_peek().type==TOK_RPAREN)depth2--;if(sc_peek().type!=TOK_RPAREN||depth2>0)sc_next();else break;}
        sc_expect(TOK_RPAREN);
        int body_start2=sc_tpos;
        if(!cond){int d=0;sc_expect(TOK_LBRACE);d=1;while(d>0&&sc_peek().type!=TOK_EOF){if(sc_peek().type==TOK_LBRACE)d++;else if(sc_peek().type==TOK_RBRACE)d--;sc_next();}return;}
        int iters2=0;
        while(cond&&!sc_error&&!sc_breaking&&iters2<10000){
            sc_tpos=body_start2;sc_block();
            /* run increment */
            int save=sc_tpos;sc_tpos=inc_start;
            /* parse increment as assignment without semicolon */
            if(sc_peek().type==TOK_IDENT){
                char vname[32];kss(vname,sc_peek().sval);sc_tpos++;
                if(sc_peek().type==TOK_ASSIGN){sc_tpos++;int v2=sc_expr();SCVar*vv=sc_getvar(vname);if(vv)vv->ival=v2;}
                else if(sc_toks[sc_tpos].type==TOK_PLUS&&sc_toks[sc_tpos+1].type==TOK_PLUS){sc_tpos+=2;SCVar*vv=sc_getvar(vname);if(vv)vv->ival++;}
                else if(sc_toks[sc_tpos].type==TOK_MINUS&&sc_toks[sc_tpos+1].type==TOK_MINUS){sc_tpos+=2;SCVar*vv=sc_getvar(vname);if(vv)vv->ival--;}
            }
            (void)save;
            sc_tpos=cond_start;cond=sc_expr();sc_expect(TOK_SEMI);
            iters2++;
        }
        sc_breaking=0;
        return;
    }

    /* break */
    if(t.type==TOK_BREAK){sc_next();sc_expect(TOK_SEMI);sc_breaking=1;return;}

    /* return */
    if(t.type==TOK_RETURN){
        sc_next();
        if(sc_peek().type!=TOK_SEMI)sc_ret_val=sc_expr();
        sc_expect(TOK_SEMI);sc_breaking=1;return;
    }

    /* expression statement (bare function call etc) */
    sc_expr();
    if(sc_peek().type==TOK_SEMI)sc_next();
}

static void sc_run_code(void){
    sc_tokenize();
    if(sc_error){term_putcolor("Tokenize error\n",AT(LRD,BLK));return;}
    sc_tpos=0;sc_breaking=0;sc_ret_val=0;
    while(!sc_error&&sc_peek().type!=TOK_EOF)sc_stmt();
    if(sc_error){term_putcolor("Error: ",AT(LRD,BLK));term_puts(sc_errmsg);term_nl();}
}

static void sc_repl(void){
    term_putcolor("SimpleC REPL v1.0 | type 'exit' to quit\n",AT(YLW,BLK));
    term_puts("  Supports: int, string, if/else, while, for, print/println\n\n");
    /* accumulate multi-line input */
    char multiline[SC_CODEMAX];int mlen=0;int in_block=0;
    while(1){
        const char*prompt=in_block?"  ... > ":"  sc> ";
        int plen=ksl(prompt);
        term_putcolor(prompt,AT(LGN,BLK));
        if(!line_read(prompt,plen))continue;
        if(ksc(line_buf,"exit")==0||ksc(line_buf,"quit")==0)break;
        if(!line_len)continue;
        /* copy to multiline buffer */
        for(int i=0;i<line_len&&mlen<SC_CODEMAX-2;i++)multiline[mlen++]=line_buf[i];
        multiline[mlen++]='\n';multiline[mlen]=0;
        /* count braces to see if complete */
        int ob=0,cb=0;
        for(int i=0;i<mlen;i++){if(multiline[i]=='{')ob++;if(multiline[i]=='}')cb++;}
        in_block=(ob>cb);
        if(!in_block){
            sc_nvar=0;/* fresh scope per REPL line */
            kmc(sc_code,multiline,mlen);sc_code_len=mlen;
            sc_run_code();
            mlen=0;multiline[0]=0;
        }
    }
    term_nl();
}

static void sc_run_file(const char*filename){
    int n=fs_find(filename,fs_cwd);
    if(n<0){term_puts("run: file not found: ");term_puts(filename);term_nl();return;}
    kmc(sc_code,fs[n].data,fs[n].size);sc_code_len=fs[n].size;
    sc_nvar=0;sc_run_code();
}

/* ════════════════════════════════════════════
   GAMES (CLI mode)
════════════════════════════════════════════ */

/* ── Snake ──────────────────────────────── */
#define SNW 60
#define SNH 18
#define SNMAX 200
static int snx[SNMAX],sny[SNMAX],snlen,sndir,sndead,snfx,snfy,snscore;
static uint32_t snlast;
static const int sndx[]={1,0,-1,0};
static const int sndy[]={0,1,0,-1};
static void sn_food(void){for(int t=0;t<300;t++){int fx=(int)(rrand()%(SNW-2))+1;int fy=(int)(rrand()%(SNH-2))+1;int ok=1;for(int i=0;i<snlen;i++)if(snx[i]==fx&&sny[i]==fy){ok=0;break;}if(ok){snfx=fx;snfy=fy;return;}}}
static void sn_init(void){rng^=ticks;snlen=4;sndir=0;sndead=0;snscore=0;snx[0]=SNW/2;sny[0]=SNH/2;for(int i=1;i<4;i++){snx[i]=snx[0]-i;sny[i]=sny[0];}sn_food();snlast=ticks;}
static void snake_run(void){
    sn_init();
    while(1){
        /* draw */
        vr_fill(1,0,VH-2,VW,' ',AT(BLK,BLK));
        int ox=10,oy=2;
        vp(oy,ox,'\xC9',AT(LGN,BLK));vf(oy,ox+1,SNW,'\xCD',AT(LGN,BLK));vp(oy,ox+SNW+1,'\xBB',AT(LGN,BLK));
        for(int r=1;r<SNH-1;r++){vp(oy+r,ox,'\xBA',AT(LGN,BLK));vf(oy+r,ox+1,SNW,sndead?'\xB0':' ',AT(sndead?DGR:BLK,BLK));vp(oy+r,ox+SNW+1,'\xBA',AT(LGN,BLK));}
        vp(oy+SNH-1,ox,'\xC8',AT(LGN,BLK));vf(oy+SNH-1,ox+1,SNW,'\xCD',AT(LGN,BLK));vp(oy+SNH-1,ox+SNW+1,'\xBC',AT(LGN,BLK));
        if(!sndead){for(int i=snlen-1;i>=0;i--)vp(oy+sny[i],ox+snx[i],i==0?'\x02':'\xDB',i==0?AT(YLW,BLK):AT(LGN,BLK));vp(oy+snfy,ox+snfx,'\x04',AT(LRD,BLK));}
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,2,"SNAKE",AT(BLK,CYN));vt(0,10,"Score:",AT(BLK,CYN));vi(0,17,snscore,AT(BLK,CYN));
        vt(0,25,"Arrows=Move  Q=Quit  R=Restart",AT(BLK,CYN));
        if(sndead){vc_str(oy+SNH/2,ox,SNW+2,"  GAME OVER! R=Restart Q=Quit  ",AT(LRD,BLK));}
        /* input */
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='q'||k=='Q')break;
            if(k=='r'||k=='R'){sn_init();}
            if(!sndead){
                if(k==K_UP&&sndir!=1)sndir=3;else if(k==K_DN&&sndir!=3)sndir=1;
                else if(k==K_LT&&sndir!=0)sndir=2;else if(k==K_RT&&sndir!=2)sndir=0;
            }
        }
        /* tick */
        if(!sndead){
            uint32_t speed=8-(uint32_t)(snscore/50);if(speed<3)speed=3;
            if(ticks-snlast>=speed){
                snlast=ticks;
                int nx=snx[0]+sndx[sndir],ny=sny[0]+sndy[sndir];
                if(nx<=0||nx>=SNW-1||ny<=0||ny>=SNH-1)sndead=1;
                else{int hit=0;for(int i=0;i<snlen-1;i++)if(snx[i]==nx&&sny[i]==ny){hit=1;break;}if(hit)sndead=1;else{for(int i=snlen-1;i>0;i--){snx[i]=snx[i-1];sny[i]=sny[i-1];}snx[0]=nx;sny[0]=ny;if(nx==snfx&&ny==snfy){snscore+=10;if(snlen<SNMAX-1)snlen++;sn_food();}}}
            }
        }
        msleep(20);
    }
    vcls();term_row=0;term_col=0;
}

/* ── Tetris ──────────────────────────────── */
#define TRW 10
#define TRH 18
static uint8_t trboard[TRH][TRW];
static int trpx,trpy,trprot,trptype,trnext,trscore,trlevel,trlines,trdead;
static uint32_t trlast;
static const int TPIECES[7][4][4][2]={
    {{{0,0},{1,0},{2,0},{3,0}},{{1,0},{1,1},{1,2},{1,3}},{{0,1},{1,1},{2,1},{3,1}},{{2,0},{2,1},{2,2},{2,3}}},
    {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}},
    {{{1,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{2,1},{1,2}},{{0,1},{1,1},{2,1},{1,2}},{{1,0},{0,1},{1,1},{1,2}}},
    {{{0,1},{1,1},{1,0},{2,0}},{{1,0},{1,1},{2,1},{2,2}},{{0,2},{1,2},{1,1},{2,1}},{{0,0},{0,1},{1,1},{1,2}}},
    {{{0,0},{1,0},{1,1},{2,1}},{{1,1},{1,2},{2,0},{2,1}},{{0,1},{1,1},{1,2},{2,2}},{{0,1},{0,2},{1,0},{1,1}}},
    {{{0,0},{0,1},{1,1},{2,1}},{{1,0},{2,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{2,2}},{{1,0},{1,1},{0,2},{1,2}}},
    {{{2,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,1},{0,2}},{{0,0},{1,0},{1,1},{1,2}}}
};
static const uint8_t TRCOLS[7]={LCY,YLW,MAG,LGN,LRD,BLU,LBL};
static int tr_valid(int px,int py,int rot,int type){for(int i=0;i<4;i++){int nx=px+TPIECES[type][rot][i][0];int ny=py+TPIECES[type][rot][i][1];if(nx<0||nx>=TRW||ny<0||ny>=TRH)return 0;if(ny>=0&&trboard[ny][nx])return 0;}return 1;}
static void tr_place(void){for(int i=0;i<4;i++){int nx=trpx+TPIECES[trptype][trprot][i][0];int ny=trpy+TPIECES[trptype][trprot][i][1];if(ny>=0&&ny<TRH&&nx>=0&&nx<TRW)trboard[ny][nx]=TRCOLS[trptype]+1;}for(int r=TRH-1;r>=0;r--){int full=1;for(int c=0;c<TRW;c++)if(!trboard[r][c]){full=0;break;}if(full){for(int rr=r;rr>0;rr--)kmc(trboard[rr],trboard[rr-1],TRW);kms(trboard[0],0,TRW);trscore+=100;trlines++;trlevel=trlines/10+1;r++;}}trptype=trnext;trnext=(int)(rrand()%7);trpx=TRW/2-2;trpy=0;trprot=0;if(!tr_valid(trpx,trpy,trprot,trptype))trdead=1;}
static void tetris_run(void){
    kms(trboard,0,sizeof(trboard));trscore=0;trlevel=1;trlines=0;trdead=0;
    trptype=(int)(rrand()%7);trnext=(int)(rrand()%7);trpx=TRW/2-2;trpy=0;trprot=0;trlast=ticks;
    while(1){
        vr_fill(0,0,VH,VW,' ',AT(BLK,BLK));
        int ox=24,oy=1;
        vp(oy,ox-1,'\xC9',AT(LGR,BLK));vf(oy,ox,TRW*2,'\xCD',AT(LGR,BLK));vp(oy,ox+TRW*2,'\xBB',AT(LGR,BLK));
        for(int r=0;r<TRH;r++){vp(oy+1+r,ox-1,'\xBA',AT(LGR,BLK));vp(oy+1+r,ox+TRW*2,'\xBA',AT(LGR,BLK));}
        vp(oy+TRH+1,ox-1,'\xC8',AT(LGR,BLK));vf(oy+TRH+1,ox,TRW*2,'\xCD',AT(LGR,BLK));vp(oy+TRH+1,ox+TRW*2,'\xBC',AT(LGR,BLK));
        for(int r=0;r<TRH;r++)for(int c=0;c<TRW;c++){uint8_t v=trboard[r][c];if(v){vp(oy+1+r,ox+c*2,'\xDB',AT(v-1,BLK));vp(oy+1+r,ox+c*2+1,'\xDB',AT(v-1,BLK));}else{vp(oy+1+r,ox+c*2,'.',AT(DGR,BLK));vp(oy+1+r,ox+c*2+1,' ',AT(BLK,BLK));}}
        if(!trdead)for(int i=0;i<4;i++){int nx=trpx+TPIECES[trptype][trprot][i][0];int ny=trpy+TPIECES[trptype][trprot][i][1];if(ny>=0){vp(oy+1+ny,ox+nx*2,'\xDB',AT(TRCOLS[trptype],BLK));vp(oy+1+ny,ox+nx*2+1,'\xDB',AT(TRCOLS[trptype],BLK));}}
        vt(2,2,"TETRIS",AT(YLW,BLK));vt(4,2,"Score:",AT(LGR,BLK));vi(4,9,trscore,AT(WHT,BLK));
        vt(5,2,"Lines:",AT(LGR,BLK));vi(5,9,trlines,AT(WHT,BLK));vt(6,2,"Level:",AT(LGR,BLK));vi(6,9,trlevel,AT(WHT,BLK));
        vt(8,2,"Next:",AT(LGR,BLK));
        for(int i=0;i<4;i++){int nx=TPIECES[trnext][0][i][0];int ny=TPIECES[trnext][0][i][1];vp(10+ny,3+nx*2,'\xDB',AT(TRCOLS[trnext],BLK));vp(10+ny,4+nx*2,'\xDB',AT(TRCOLS[trnext],BLK));}
        vt(15,2,"</> Move",AT(DGR,BLK));vt(16,2,"^ Rotate",AT(DGR,BLK));vt(17,2,"v Drop",AT(DGR,BLK));vt(18,2,"Q Quit",AT(DGR,BLK));
        if(trdead)vc_str(oy+TRH/2,ox-1,TRW*2+2,"GAME OVER",AT(LRD,BLK));
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='q'||k=='Q')break;
            if(trdead){if(k=='r'||k=='R'){kms(trboard,0,sizeof(trboard));trscore=0;trlevel=1;trlines=0;trdead=0;trptype=(int)(rrand()%7);trnext=(int)(rrand()%7);trpx=TRW/2-2;trpy=0;trprot=0;}}
            else{
                if(k==K_LT&&tr_valid(trpx-1,trpy,trprot,trptype))trpx--;
                else if(k==K_RT&&tr_valid(trpx+1,trpy,trprot,trptype))trpx++;
                else if(k==K_DN&&tr_valid(trpx,trpy+1,trprot,trptype)){trpy++;trscore++;}
                else if(k==K_UP){int nr=(trprot+1)%4;if(tr_valid(trpx,trpy,nr,trptype))trprot=nr;}
                else if(k==' '){while(tr_valid(trpx,trpy+1,trprot,trptype)){trpy++;trscore+=2;}tr_place();}
            }
        }
        if(!trdead){uint32_t speed=50/(uint32_t)(trlevel>10?10:trlevel);if(speed<5)speed=5;if(ticks-trlast>=speed){trlast=ticks;if(tr_valid(trpx,trpy+1,trprot,trptype))trpy++;else tr_place();}}
        msleep(15);
    }
    vcls();term_row=0;term_col=0;
}

/* ── Pong ────────────────────────────────── */
static void pong_run(void){
    int bx=35,by=10,bdx=1,bdy=1;
    int py=8,score=0,lives=3;
    uint32_t last=ticks;
    while(1){
        vr_fill(0,0,VH,VW,' ',AT(BLK,BLK));
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,2,"PONG",AT(BLK,CYN));vt(0,10,"Score:",AT(BLK,CYN));vi(0,17,score,AT(BLK,CYN));
        vt(0,25,"Lives:",AT(BLK,CYN));for(int i=0;i<lives;i++)vp(0,32+i,'\x03',AT(LRD,CYN));
        vt(0,38,"W/S=Paddle  Q=Quit",AT(BLK,CYN));
        /* field */
        for(int r=1;r<VH-1;r++){vp(r,0,'\xBA',AT(DGR,BLK));vp(r,VW-1,'\xBA',AT(DGR,BLK));}
        vf(1,0,VW,'\xC4',AT(DGR,BLK));vf(VH-2,0,VW,'\xC4',AT(DGR,BLK));
        for(int r=2;r<VH-2;r+=2)vp(r,VW/2,'\xB0',AT(DGR,BLK));
        /* paddle */
        for(int i=0;i<5;i++)vp(2+py+i,3,'\xDB',AT(LCY,BLK));
        /* ball */
        vp(2+by,4+bx,'O',AT(YLW,BLK));
        if(kt!=kh){uint8_t k=kgetc();if(k=='q'||k=='Q')break;if((k=='w'||k=='W')&&py>0)py--;if((k=='s'||k=='S')&&py<VH-7)py++;}
        if(ticks-last>=3){last=ticks;bx+=bdx;by+=bdy;
            if(by<=0){by=0;bdy=1;}if(by>=VH-5){by=VH-5;bdy=-1;}
            if(bx>=VW-6){bx=VW-6;bdx=-1;score+=5;}
            if(bx<=0){lives--;bx=35;by=10;bdx=1;bdy=1;if(lives<=0)break;}
            if(bx<=1&&by>=py&&by<=py+5){bx=2;bdx=1;score+=2;bdy=(by-py<3)?-1:1;}
        }
        msleep(15);
    }
    vcls();term_row=0;term_col=0;
}

/* ── 2048 ────────────────────────────────── */
static int g2b[4][4];
static int g2score,g2done;
static void g2_add(void){int e[16][2];int n=0;for(int r=0;r<4;r++)for(int c=0;c<4;c++)if(!g2b[r][c]){e[n][0]=r;e[n][1]=c;n++;}if(!n)return;int i=(int)(rrand()%n);g2b[e[i][0]][e[i][1]]=(rrand()%4==0)?4:2;}
static void g2_slide(int dir){int moved=0;if(dir==0||dir==2){for(int r=0;r<4;r++){int row[4]={0};int ri=0;if(dir==0){for(int c=0;c<4;c++)if(g2b[r][c])row[ri++]=g2b[r][c];}else{for(int c=3;c>=0;c--)if(g2b[r][c])row[ri++]=g2b[r][c];}for(int i=0;i<3;i++)if(row[i]&&row[i]==row[i+1]){row[i]*=2;g2score+=row[i];row[i+1]=0;}int tmp[4]={0};ri=0;for(int i=0;i<4;i++)if(row[i])tmp[ri++]=row[i];if(dir==0)for(int c=0;c<4;c++){if(g2b[r][c]!=tmp[c])moved=1;g2b[r][c]=tmp[c];}else for(int c=0;c<4;c++){if(g2b[r][3-c]!=tmp[c])moved=1;g2b[r][3-c]=tmp[c];}}}else{for(int c=0;c<4;c++){int col[4]={0};int ri=0;if(dir==1){for(int r=0;r<4;r++)if(g2b[r][c])col[ri++]=g2b[r][c];}else{for(int r=3;r>=0;r--)if(g2b[r][c])col[ri++]=g2b[r][c];}for(int i=0;i<3;i++)if(col[i]&&col[i]==col[i+1]){col[i]*=2;g2score+=col[i];col[i+1]=0;}int tmp[4]={0};ri=0;for(int i=0;i<4;i++)if(col[i])tmp[ri++]=col[i];if(dir==1)for(int r=0;r<4;r++){if(g2b[r][c]!=tmp[r])moved=1;g2b[r][c]=tmp[r];}else for(int r=0;r<4;r++){if(g2b[3-r][c]!=tmp[r])moved=1;g2b[3-r][c]=tmp[r];}}}if(moved)g2_add();int ok=0;for(int r=0;r<4;r++)for(int c=0;c<4;c++){if(!g2b[r][c])ok=1;else{if(c<3&&g2b[r][c]==g2b[r][c+1])ok=1;if(r<3&&g2b[r][c]==g2b[r+1][c])ok=1;}}if(!ok)g2done=1;}
static const uint8_t G2C[]={LGR,YLW,LBL,LGN,LCY,LRD,PNK,MAG,RED,GRN,CYN,BLU};
static void g2048_run(void){
    kms(g2b,0,sizeof(g2b));g2score=0;g2done=0;g2_add();g2_add();
    while(1){
        vr_fill(0,0,VH,VW,' ',AT(BLK,BLK));
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,2,"2048",AT(BLK,CYN));vt(0,9,"Score:",AT(BLK,CYN));vi(0,16,g2score,AT(BLK,CYN));
        vt(0,25,"Arrows=Slide  Q=Quit  R=New",AT(BLK,CYN));
        int ox=24,oy=3;
        for(int r=0;r<4;r++)for(int c=0;c<4;c++){
            int v=g2b[r][c];int dr=oy+r*5,dc=ox+c*11;
            uint8_t a=v?AT(BLK,G2C[__builtin_ctz(v>1?v:1)%12]):AT(DGR,DGR);
            for(int i=0;i<4;i++)vf(dr+i,dc,10,' ',a);
            if(v){char buf[8];ksi(buf,v);vc_str(dr+1,dc,10,buf,a);}
        }
        if(g2done)vc_str(12,0,VW,"GAME OVER! R=Restart",AT(LRD,BLK));
        if(kt!=kh){uint8_t k=kgetc();if(k=='q'||k=='Q')break;if(k=='r'||k=='R'){kms(g2b,0,sizeof(g2b));g2score=0;g2done=0;g2_add();g2_add();}else if(!g2done){if(k==K_LT)g2_slide(0);else if(k==K_RT)g2_slide(2);else if(k==K_UP)g2_slide(1);else if(k==K_DN)g2_slide(3);}}
        msleep(30);
    }
    vcls();term_row=0;term_col=0;
}

/* ── Hangman ─────────────────────────────── */
static const char*HMWORDS[]={"kernel","bootloader","assembly","register","interrupt","processor","memory","pointer","stack","compiler","variable","function","algorithm","recursion","debugger","protocol","interface","structure","filesystem","scheduler"};
#define NHWORDS 20
static char hmword[20];
static char hmguessed[27];
static int hmwrong,hmdone,hmwon;
static void hang_run(void){
    kss(hmword,HMWORDS[(int)(rrand()%NHWORDS)]);
    kms(hmguessed,0,27);hmwrong=0;hmdone=0;hmwon=0;
    while(1){
        vcls();
        static const char*gal[7][6]={
            {"     ","     ","     ","     ","     ","|===="},
            {"  |  ","  |  ","     ","     ","     ","|===="},
            {" _|  ","  |  ","     ","     ","     ","|===="},
            {" _|_ ","  |  ","     ","     ","     ","|===="},
            {" _|_ "," (O) ","     ","     ","     ","|===="},
            {" _|_ "," (O) ","  |  ","     ","     ","|===="},
            {" _|_ "," (O) "," /|\\ "," / \\ ","     ","|===="}
        };
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,2,"HANGMAN",AT(BLK,CYN));vt(0,15,"a-z=Guess  Q=Quit",AT(BLK,CYN));
        int stage=hmwrong>6?6:hmwrong;
        for(int i=0;i<6;i++)vt(3+i,4,gal[stage][i],AT(WHT,BLK));
        int wl=ksl(hmword);
        for(int i=0;i<wl;i++){
            int rev=0;for(int j=0;j<26;j++)if(hmguessed[j]==hmword[i]){rev=1;break;}
            vp(12,15+i*2,rev?hmword[i]:'_',AT(YLW,BLK));
        }
        term_row=14;term_col=0;
        term_puts("Guessed: ");
        for(int i=0;i<26;i++)if(hmguessed[i]){term_putc(hmguessed[i]);term_putc(' ');}
        term_nl();
        term_puts("Wrong: ");term_puti(hmwrong);term_puts("/6\n");
        if(hmwon){term_putcolor("YOU WIN! Q=Quit R=New\n",AT(LGN,BLK));}
        else if(hmdone){term_putcolor("GAME OVER! Word was: ",AT(LRD,BLK));term_puts(hmword);term_nl();term_puts("R=New Q=Quit\n");}
        uint8_t k=kgetc();
        if(k=='q'||k=='Q')break;
        if(k=='r'||k=='R'){kss(hmword,HMWORDS[(int)(rrand()%NHWORDS)]);kms(hmguessed,0,27);hmwrong=0;hmdone=0;hmwon=0;continue;}
        if(hmdone)continue;
        if(k>='a'&&k<='z'){
            int dup=0;for(int i=0;i<26;i++)if(hmguessed[i]==k){dup=1;break;}
            if(!dup){for(int i=0;i<26;i++)if(!hmguessed[i]){hmguessed[i]=k;break;}
                int found=0;for(int i=0;i<wl;i++)if(hmword[i]==k)found++;
                if(!found)hmwrong++;
                int win2=1;for(int i=0;i<wl;i++){int r=0;for(int j=0;j<26;j++)if(hmguessed[j]==hmword[i]){r=1;break;}if(!r){win2=0;break;}}
                if(win2){hmdone=1;hmwon=1;}if(hmwrong>=6)hmdone=1;
            }
        }
    }
    vcls();term_row=0;term_col=0;
}

/* ── Tic Tac Toe ─────────────────────────── */
static char tttb[9];
static void ttt_cpu(void){const int w[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};for(int pass=0;pass<2;pass++){char c2=pass?'X':'O';for(int i=0;i<8;i++){int a=w[i][0],b=w[i][1],cc=w[i][2];if(tttb[a]==c2&&tttb[b]==c2&&!tttb[cc]){tttb[cc]='O';return;}if(tttb[a]==c2&&tttb[cc]==c2&&!tttb[b]){tttb[b]='O';return;}if(tttb[b]==c2&&tttb[cc]==c2&&!tttb[a]){tttb[a]='O';return;}}}if(!tttb[4]){tttb[4]='O';return;}int ord[]={0,2,6,8,1,3,5,7};for(int i=0;i<8;i++)if(!tttb[ord[i]]){tttb[ord[i]]='O';return;}}
static int ttt_check(void){const int w[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};for(int i=0;i<8;i++)if(tttb[w[i][0]]&&tttb[w[i][0]]==tttb[w[i][1]]&&tttb[w[i][1]]==tttb[w[i][2]])return 1;int full=1;for(int i=0;i<9;i++)if(!tttb[i]){full=0;break;}return full?2:0;}
static void ttt_run(void){
    kms(tttb,0,9);int sel=4;
    while(1){
        vcls();
        vf(0,0,VW,' ',AT(BLK,CYN));vt(0,2,"TIC TAC TOE vs CPU",AT(BLK,CYN));vt(0,28,"Arrows=Move Enter=Place Q=Quit",AT(BLK,CYN));
        for(int i=0;i<3;i++)for(int j=0;j<3;j++){
            int idx=i*3+j;int r=5+i*4,c=28+j*9;
            int is_sel=(idx==sel);
            uint8_t a=is_sel?AT(YLW,DGR):AT(WHT,BLK);
            for(int rr=r;rr<r+3;rr++)vf(rr,c,7,' ',a);
            vp(r+1,c+3,tttb[idx]?tttb[idx]:' ',tttb[idx]=='X'?AT(LRD,a>>4):AT(LGN,a>>4));
            if(j<2)for(int rr=r;rr<r+3;rr++)vp(rr,c+7,'|',AT(DGR,BLK));
            if(i<2)vf(r+3,c,7,'\xC4',AT(DGR,BLK));
        }
        int res=ttt_check();
        term_row=16;term_col=0;
        if(res==1)term_puts(tttb[4]=='X'||(!tttb[4]&&tttb[0]=='X')?"You win!":"CPU wins!");
        else if(res==2)term_puts("Draw!");
        else term_puts("Your turn (X) - Arrows+Enter");
        term_nl();term_puts("R=New  Q=Quit");
        uint8_t k=kgetc();
        if(k=='q'||k=='Q')break;
        if(k=='r'||k=='R'){kms(tttb,0,9);sel=4;continue;}
        if(res)continue;
        if(k==K_UP&&sel>=3)sel-=3;else if(k==K_DN&&sel<=5)sel+=3;
        else if(k==K_LT&&sel%3>0)sel--;else if(k==K_RT&&sel%3<2)sel++;
        else if(k==K_ENT&&!tttb[sel]){tttb[sel]='X';if(!ttt_check()){ttt_cpu();}}
    }
    vcls();term_row=0;term_col=0;
}

/* ── Number Guess ────────────────────────── */
static void guess_run(void){
    rng^=ticks;int target=(int)(rrand()%100)+1;int guesses=0;
    term_puts("Guess a number 1-100. 'q' to quit.\n");
    while(1){
        term_putcolor("Guess: ",AT(LGN,BLK));
        if(!line_read("Guess: ",7))continue;
        if(ksc(line_buf,"q")==0)break;
        if(!line_len)continue;
        int n=katoi(line_buf);guesses++;
        if(n==target){term_puti(n);term_puts(" - Correct! ");term_puti(guesses);term_puts(" guesses.\n");break;}
        else if(n<target)term_puts("Too low! Go higher.\n");
        else term_puts("Too high! Go lower.\n");
    }
}

/* ════════════════════════════════════════════
   GUI DESKTOP (type 'gui' to enter)
════════════════════════════════════════════ */

/* ════════════════════════════════════════════
   MUSIC PLAYER - Multiple genres
════════════════════════════════════════════ */
typedef struct{uint32_t freq;uint32_t dur;}Note;

/* Frequencies */
#define _C4  262
#define _D4  294
#define _E4  330
#define _F4  349
#define _G4  392
#define _A4  440
#define _B4  494
#define _C5  523
#define _D5  587
#define _E5  659
#define _F5  698
#define _G5  784
#define _A5  880
#define _A3  220
#define _B3  247
#define _G3  196
#define _F3  175
#define _E3  165
#define _D3  147
#define _C3  131
#define _REST 0
#define _AS4 466
#define _GS4 415
#define _DS4 311
#define _CS4 277

static void play_note(uint32_t freq,uint32_t dur){
    if(freq==0){msleep(dur);}
    else{spk_on(freq);msleep(dur);spk_off();msleep(20);}
}
static void play_melody(const Note*notes,int n){
    for(int i=0;i<n;i++){
        if(kt!=kh)break;/* any key stops */
        play_note(notes[i].freq,notes[i].dur);
    }
    spk_off();
}

/* --- Mario Theme (Western/Pop) --- */
static const Note MARIO[]={
    {_E5,120},{_E5,120},{_REST,60},{_E5,120},{_REST,60},{_C5,120},{_E5,120},
    {_G5,240},{_REST,240},{_G4,240},{_REST,240},
    {_C5,180},{_G4,120},{_REST,120},{_E4,180},{_A4,120},{_B4,120},
    {_AS4,60},{_A4,120},{_G4,90},{_E5,90},{_G5,90},{_A5,180},{_F5,90},{_G5,90},
    {_REST,60},{_E5,120},{_C5,90},{_D5,90},{_B4,180}
};

/* --- Beethoven 5th (Western Classical) --- */
static const Note BEETHOVEN[]={
    {_G4,150},{_G4,150},{_G4,150},{_DS4,600},
    {_REST,150},{_F4,150},{_F4,150},{_F4,150},{_D4,600},
    {_REST,300},{_G4,150},{_G4,150},{_G4,150},{_DS4,600},
    {_REST,150},{_F4,150},{_F4,150},{_F4,150},{_D4,600}
};

/* --- Raga Bhairav (Indian Classical - Carnatic/Hindustani) --- */
static const Note BHAIRAV[]={
    {_C4,300},{_CS4,150},{_E4,300},{_F4,300},{_G4,300},
    {_GS4,150},{_B4,300},{_C5,600},{_REST,150},
    {_C5,300},{_B4,150},{_GS4,300},{_G4,300},{_F4,300},
    {_E4,150},{_CS4,300},{_C4,600},{_REST,300},
    {_G4,200},{_F4,200},{_E4,200},{_CS4,400},{_C4,600}
};

/* --- Raga Yaman (Hindustani Classical) --- */
static const Note YAMAN[]={
    {_F4,400},{_A4,200},{_B4,200},{_C5,400},{_D5,200},{_E5,200},
    {_D5,400},{_C5,200},{_B4,200},{_A4,400},{_REST,200},
    {_D5,300},{_C5,200},{_B4,300},{_A4,200},{_G4,300},{_F4,200},
    {_E4,200},{_F4,600},{_REST,300},
    {_C5,200},{_B4,200},{_A4,200},{_G4,200},{_F4,400}
};

/* --- Carnatic Raga Kalyani --- */
static const Note KALYANI[]={
    {_C4,250},{_E4,250},{_F4,125},{_G4,125},{_A4,250},
    {_B4,250},{_C5,500},{_REST,125},
    {_A4,250},{_G4,250},{_E4,250},{_D4,125},{_E4,125},
    {_C4,500},{_REST,250},
    {_G4,200},{_A4,200},{_B4,200},{_C5,400},
    {_B4,200},{_A4,200},{_G4,200},{_E4,400},{_C4,600}
};

/* --- Pop (Happy Birthday style) --- */
static const Note HAPPY_BD[]={
    {_C4,200},{_C4,100},{_D4,300},{_C4,300},{_F4,300},{_E4,600},
    {_C4,200},{_C4,100},{_D4,300},{_C4,300},{_G4,300},{_F4,600},
    {_C4,200},{_C4,100},{_C5,300},{_A4,300},{_F4,300},{_E4,300},{_D4,600},
    {_AS4,200},{_AS4,100},{_A4,300},{_F4,300},{_G4,300},{_F4,600}
};

/* --- Rap Beat (rhythmic PC speaker) --- */
static const Note RAP_BEAT[]={
    {_C3,100},{_REST,50},{_C3,100},{_REST,100},{_G3,100},{_REST,50},
    {_C3,100},{_REST,50},{_C3,50},{_C3,50},{_REST,100},{_F3,100},{_REST,100},
    {_C3,100},{_REST,50},{_C3,100},{_REST,100},{_A3,100},{_REST,50},
    {_G3,100},{_REST,50},{_F3,100},{_REST,50},{_E3,100},{_REST,100},
    {_C3,80},{_D3,80},{_E3,80},{_F3,80},{_G3,160},{_REST,80},
    {_C3,80},{_D3,80},{_E3,80},{_F3,80},{_E3,80},{_D3,80},{_C3,160}
};

/* --- Twinkle Twinkle (for fun) --- */
static const Note TWINKLE[]={
    {_C4,300},{_C4,300},{_G4,300},{_G4,300},{_A4,300},{_A4,300},{_G4,600},
    {_F4,300},{_F4,300},{_E4,300},{_E4,300},{_D4,300},{_D4,300},{_C4,600},
    {_G4,300},{_G4,300},{_F4,300},{_F4,300},{_E4,300},{_E4,300},{_D4,600},
    {_G4,300},{_G4,300},{_F4,300},{_F4,300},{_E4,300},{_E4,300},{_D4,600}
};

#define NMELODY 7
static const char*MELODY_NAMES[NMELODY]={
    "1. Mario Theme (Pop/Game)",
    "2. Beethoven 5th (Western Classical)",
    "3. Indian Classical Melody I",
    "4. Indian Classical Melody II",
    "5. Indian Classical Melody III",
    "6. Happy Birthday (Pop)",
    "7. Rap Beat (Rhythmic)"
};

static void music_run(void){
    vcls();
    vf(0,0,VW,' ',AT(BLK,MAG));
    vc_str(0,0,VW,"MAYURA OS - Music Player",AT(WHT,MAG));
    vf(VH-1,0,VW,' ',AT(BLK,MAG));
    vt(VH-1,2,"Press number to play | Q=Quit | Any key stops current",AT(WHT,MAG));
    /* draw menu */
    vr_fill(1,0,VH-2,VW,' ',AT(BLK,BLK));
    vc_str(2,0,VW,"Select a genre:",AT(YLW,BLK));
    for(int i=0;i<NMELODY;i++){
        uint8_t a=(i%2==0)?AT(LCY,BLK):AT(LGN,BLK);
        vt(4+i*2,10,MELODY_NAMES[i],a);
    }
    vc_str(20,0,VW,"* All melodies use PC speaker *",AT(DGR,BLK));
    while(1){
        uint8_t k=kgetc();
        if(k=='q'||k=='Q')break;
        /* clear playing area */
        vf(22,0,VW,' ',AT(BLK,BLK));
        const Note*melody=0;int nlen=0;const char*genre="";
        if(k=='1'){melody=MARIO;nlen=sizeof(MARIO)/sizeof(Note);genre="Mario Theme";}
        else if(k=='2'){melody=BEETHOVEN;nlen=sizeof(BEETHOVEN)/sizeof(Note);genre="Beethoven 5th";}
        else if(k=='3'){melody=BHAIRAV;nlen=sizeof(BHAIRAV)/sizeof(Note);genre="Indian Classical I";}
        else if(k=='4'){melody=YAMAN;nlen=sizeof(YAMAN)/sizeof(Note);genre="Indian Classical II";}
        else if(k=='5'){melody=KALYANI;nlen=sizeof(KALYANI)/sizeof(Note);genre="Indian Classical III";}
        else if(k=='6'){melody=HAPPY_BD;nlen=sizeof(HAPPY_BD)/sizeof(Note);genre="Happy Birthday";}
        else if(k=='7'){melody=RAP_BEAT;nlen=sizeof(RAP_BEAT)/sizeof(Note);genre="Rap Beat";}
        if(melody){
            vf(22,0,VW,' ',AT(BLK,GRN));
            vt(22,2,"Now playing: ",AT(BLK,GRN));
            vt(22,15,genre,AT(YLW,GRN));
            vt(22,15+ksl(genre),"  (any key to stop)",AT(BLK,GRN));
            play_melody(melody,nlen);
            vf(22,0,VW,' ',AT(BLK,BLK));
            vc_str(22,0,VW,"Done. Press number to play again.",AT(DGR,BLK));
        }
    }
    vcls();term_row=0;term_col=0;
}

/* ════════════════════════════════════════════
   CHATBOT - Simple pattern matching AI
════════════════════════════════════════════ */
static void chat_run(void){
    vcls();
    vf(0,0,VW,' ',AT(BLK,BLU));
    vc_str(0,0,VW,"MAYURA AI - Chat (type 'bye' to exit)",AT(WHT,BLU));
    vf(VH-1,0,VW,' ',AT(BLK,LGR));
    vt(VH-1,2,"Type anything and press Enter",AT(DGR,LGR));
    term_row=2;term_col=0;
    term_attr=LGR|(BLK<<4);

    /* greeting */
    term_putcolor("MayuraAI: ",AT(LGN,BLK));
    term_puts("Hello! I am MayuraAI, your bare-metal assistant.\n");
    term_putcolor("MayuraAI: ",AT(LGN,BLK));
    term_puts("I was created by Susanth. Ask me anything!\n\n");

    /* pattern -> response table */
    static const char*PATTERNS[]={
        "hello","hi","hey","namaste","vanakkam",
        "how are you","how r u","wassup","sup",
        "your name","who are you","what are you",
        "susanth","creator","who made you","who built",
        "what is mayura","about mayura","this os",
        "carnatic","music","raga","classical",
        "snake","game","games","play",
        "simplec","code","programming","sc",
        "time","clock","date",
        "help","commands","what can you do",
        "joke","funny","laugh",
        "sad","depressed","unhappy",
        "happy","good","great","awesome",
        "india","indian","bharat",
        "linux","unix","os","kernel",
        "bye","exit","quit","goodbye",
        "love","like","cool",
        "weather","rain","sun",
        "god","pray",
        "age","old","young",
        "smart","intelligent","ai","robot",
        (const char*)0
    };
    static const char*RESPONSES[]={
        "Hello there! How can I help you today?",
        "Hey! Great to see you. What is on your mind?",
        "Hey! Feeling energetic today!",
        "Hello! Nice to meet you!",
        "Hi there! Great to chat!",
        "I am doing great, running on bare metal! No lag at all.",
        "I am fantastic! Just electrons and logic gates here.",
        "Not much, just processing instructions at full speed!",
        "All good! Ask me something interesting.",
        "I am MayuraAI, the chatbot of Mayura OS v1.0!",
        "I am MayuraAI - an AI assistant built into Mayura OS.",
        "I am a pattern-matching chatbot living in bare metal RAM.",
        "Susanth is my creator - a brilliant developer!",
        "Susanth built this entire OS from scratch. Impressive right?",
        "I was made by Susanth using pure C and NASM assembly.",
        "Susanth created me. No libraries, no OS beneath, just silicon.",
        "Mayura OS is a bare-metal x86 OS with 100+ commands and games!",
        "Mayura OS boots directly on hardware with no OS underneath.",
        "This OS has a shell, SimpleC language, games, and even a chatbot!",
        "Indian classical music is divine! Mayura OS can play Ragas!",
        "Type 'music' to play Indian classical and western melodies!",
        "Music is beautiful! Try the music player!",
        "Great taste! Try 'music' to hear some melodies!",
        "Snake is my favourite game too! Type 'snake' to play.",
        "Games available: snake tetris pong 2048 hang ttt mines guess",
        "Type a game name to play it! snake, tetris, pong, 2048...",
        "SimpleC is our built-in language. Type 'sc' to try it!",
        "You can code in SimpleC! int x=5; println(x*2); Try it!",
        "Type 'sc' for SimpleC REPL or 'run file.sc' for a script.",
        "Type 'date' for current IST time and date.",
        "Current time is shown in the shell prompt. Type 'clock' for big digits!",
        "Type 'date' to see the current IST date and time.",
        "Type 'help' to see all 100+ available commands.",
        "I can chat, play music, show system info, run games, and more!",
        "Try: snake tetris piano matrix fire neofetch fortune cowsay",
        "Why did the computer go to therapy? It had too many bugs!",
        "Why do programmers prefer dark mode? Because light attracts bugs!",
        "I am sorry to hear that. Remember: this too shall pass. Om Shanti.",
        "Cheer up! Type 'fortune' for a motivational quote.",
        "That is wonderful! Keep spreading positivity!",
        "Glad to hear it! What would you like to do?",
        "Amazing! The positive energy powers my CPU!",
        "India - land of Vedas, mathematics, and great programmers!",
        "Jai Hind! India gave the world zero, yoga, and chess!",
        "Great country! Home of Carnatic music, Ragas, and biriyani.",
        "Linux is amazing! Mayura OS takes inspiration from its CLI feel.",
        "Unix philosophy: do one thing well. Mayura OS tries to do many things well!",
        "This kernel is i686 bare metal - no Linux, no Windows, just pure code.",
        "Goodbye! May your code always compile on the first try!",
        "See you! Come back anytime, I will be here in RAM.",
        "Farewell! Remember to reboot if things get weird.",
        "Goodbye! Om Namo Venkatesaya!",
        "Aww thank you! I love you too (in a computer-science way)!",
        "Cool things about this OS: 100 commands, SimpleC, music, chatbot!",
        "That is really cool! This OS has some cool features too.",
        "Weather module says: always sunny inside Mayura OS!",
        "No network here, but type 'weather' for simulated forecast!",
        "It is always perfect weather inside a computer.",
        "That is a deep question! The universe is made of code.",
        "Philosophy and code - both seek the truth.",
        "Interesting topic. Keep exploring!",
        "This OS is eternally young - just 2133 lines of C!",
        "Age is just a number. This kernel boots in under 2 seconds!",
        "Young and fast! Boots faster than your other OS.",
        "Thank you! I try my best with simple pattern matching.",
        "I am not GPT, but I give it my best shot!",
        "AI in bare metal! No GPU needed, just a 486 and some RAM.",
        (const char*)0
    };

    while(1){
        term_putcolor("\nYou: ",AT(YLW,BLK));
        if(!line_read("You: ",5))continue;
        if(!line_len)continue;
        /* check bye */
        char lb[LINE_MAX];kss(lb,line_buf);klower(lb);
        if(ksc(lb,"bye")==0||ksc(lb,"exit")==0||ksc(lb,"quit")==0)break;
        /* find matching pattern */
        int found=-1;
        for(int i=0;PATTERNS[i]!=(const char*)0;i++){
            if(ksl(PATTERNS[i])>0){
                /* check if pattern appears in input */
                int pl=ksl(PATTERNS[i]);
                for(int j=0;lb[j];j++){
                    if(knc(lb+j,PATTERNS[i],pl)==0){found=i;break;}
                }
            }
            if(found>=0)break;
        }
        term_putcolor("\nMayuraAI: ",AT(LGN,BLK));
        if(found>=0)term_puts(RESPONSES[found]);
        else{
            /* default responses */
            static const char*DEF[]={"Interesting! Tell me more.","I see! That is fascinating.","Hmm, I am still learning about that.","Great point! I will think about that.","Could you rephrase that? My vocabulary is limited!","Intriguing! Type 'help' if you need commands."};
            term_puts(DEF[(int)(rrand()%6)]);
        }
        term_nl();
    }
    term_nl();
    term_putcolor("MayuraAI: ",AT(LGN,BLK));
    term_puts("Goodbye! Have a great time!\n\n");
    vcls();term_row=0;term_col=0;
}

/* ════════════════════════════════════════════
   SCREENSAVER - Multiple modes
════════════════════════════════════════════ */
static void saver_run(void){
    vcls();
    vf(0,0,VW,' ',AT(BLK,DGR));
    vc_str(0,0,VW,"Screensaver - 1=Matrix 2=Fire 3=Stars 4=Bounce 5=Rain Q=Quit",AT(LGR,DGR));
    vf(VH-1,0,VW,' ',AT(BLK,DGR));
    /* default: bouncing blocks */
    int bx[8],by[8],bdx[8],bdy[8];
    uint8_t bc[8]={LRD,LGN,LCY,YLW,PNK,LBL,MAG,WHT};
    for(int i=0;i<8;i++){bx[i]=(int)(rrand()%(VW-4))+2;by[i]=(int)(rrand()%(VH-4))+2;bdx[i]=(int)(rrand()%2)?1:-1;bdy[i]=(int)(rrand()%2)?1:-1;}
    int mode=4;
    while(1){
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='q'||k=='Q'){break;}
            if(k>='1'&&k<='5'){mode=k-'0';vcls_color(AT(BLK,BLK));}
        }
        if(mode==1){/* matrix */
            static int col_pos2[VW];static int col_sp2[VW];
            static int minit=0;
            if(!minit){for(int i=0;i<VW;i++){col_pos2[i]=(int)(rrand()%VH);col_sp2[i]=(int)(rrand()%3)+1;}minit=1;}
            static const char MCH[]="abcdefghijklmnopqrstuvwxyz0123456789@#$%";
            int mc2=ksl(MCH);
            for(int c=0;c<VW;c++){if((int)(rrand()%col_sp2[c])==0){int r=col_pos2[c];vp(r,c,MCH[(int)(rrand()%mc2)],AT(LGN,BLK));if(r>0)vp(r-1,c,MCH[(int)(rrand()%mc2)],AT(DGR,BLK));if(r>1)vp(r-2,c,' ',AT(BLK,BLK));col_pos2[c]=(r+1)%VH;}}
        } else if(mode==2){/* fire */
            static uint8_t fbuf[VH][VW];static uint8_t finit=0;
            if(!finit){kms(fbuf,0,sizeof(fbuf));finit=1;}
            static const uint8_t FC[]={BLK,RED,LRD,YLW,WHT};
            for(int c=0;c<VW;c++)fbuf[VH-1][c]=(uint8_t)(rrand()%2?4:3);
            for(int r=0;r<VH-1;r++)for(int c=0;c<VW;c++){int s=(int)fbuf[r+1][(c-1+VW)%VW]+(int)fbuf[r+1][c]+(int)fbuf[r+1][(c+1)%VW];int v=s/3;if(v>0&&(int)(rrand()%3)==0)v--;fbuf[r][c]=(uint8_t)v;vp(r,c,fbuf[r][c]>0?'\xDB':' ',AT(FC[fbuf[r][c]<5?fbuf[r][c]:4],BLK));}
        } else if(mode==3){/* stars */
            static int sz[60],sy2[60],szz[60];static int sinit=0;
            if(!sinit){for(int i=0;i<60;i++){sz[i]=(int)(rrand()%(VW-2))+1;sy2[i]=(int)(rrand()%(VH-2))+1;szz[i]=(int)(rrand()%3)+1;}sinit=1;}
            vcls_color(AT(BLK,BLK));
            for(int i=0;i<60;i++){vp(sy2[i],sz[i],szz[i]==3?'.':szz[i]==2?'+':'*',AT(szz[i]==3?DGR:szz[i]==2?LGR:WHT,BLK));sz[i]--;if(sz[i]<0){sz[i]=VW-1;sy2[i]=(int)(rrand()%(VH-2))+1;szz[i]=(int)(rrand()%3)+1;}}
        } else if(mode==4){/* bounce */
            vcls_color(AT(BLK,BLK));
            /* draw mayura os text bouncing */
            static int tx=20,ty=10,tdx=1,tdy=1;
            tx+=tdx;ty+=tdy;
            if(tx<=0||tx>=VW-12)tdx=-tdx;
            if(ty<=1||ty>=VH-2)tdy=-tdy;
            uint8_t tc=AT(bc[(int)(rrand()%8)],BLK);
            vt(ty,tx,"MAYURA OS",tc);
            for(int i=0;i<8;i++){bx[i]+=bdx[i];by[i]+=bdy[i];if(bx[i]<=0||bx[i]>=VW-2)bdx[i]=-bdx[i];if(by[i]<=1||by[i]>=VH-2)bdy[i]=-bdy[i];vp(by[i],bx[i],'\xDB',AT(bc[i],BLK));vp(by[i],bx[i]+1,'\xDB',AT(bc[i],BLK));}
        } else if(mode==5){/* rain */
            static int rdrop[VW];static uint32_t rlast=0;
            if(ticks-rlast>3){rlast=ticks;
                for(int c=0;c<VW;c++){if(rdrop[c]>0){vp(rdrop[c]-1,c,' ',AT(BLK,BLK));if(rdrop[c]<VH-1){vp(rdrop[c],c,'|',AT(LBL,BLK));rdrop[c]++;}else{vp(rdrop[c],c,' ',AT(BLK,BLK));rdrop[c]=0;}}else if((int)(rrand()%8)==0)rdrop[c]=1;}
            }
        }
        msleep(40);
    }
    vcls();term_row=0;term_col=0;
}

/* ════════════════════════════════════════════
   SYSINFO - Beautiful dashboard
════════════════════════════════════════════ */
static void sysinfo_run(void){
    while(1){
        vcls_color(AT(BLK,BLK));
        /* ── Top banner ── */
        vf(0,0,VW,'\xCD',AT(LCY,BLK));
        vc_str(0,0,VW,"\xB5 MAYURA OS v1.0 - SYSTEM DASHBOARD \xC6",AT(YLW,BLK));
        vf(VH-1,0,VW,'\xCD',AT(LCY,BLK));
        vt(VH-1,2,"Q=Quit  R=Refresh",AT(DGR,BLK));

        uint8_t h,m,s;rtc_ist(&h,&m,&s);
        uint8_t day,mon,yr;rtc_date(&day,&mon,&yr);
        static const char*MON[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

        /* ── Left panel: System Info ── */
        vp(1,0,'\xC9',AT(LCY,BLK));vf(1,1,37,'\xCD',AT(LCY,BLK));vp(1,38,'\xBB',AT(LCY,BLK));
        vp(2,0,'\xBA',AT(LCY,BLK));vc_str(2,1,37,"  SYSTEM INFO  ",AT(YLW,BLK));vp(2,38,'\xBA',AT(LCY,BLK));
        vp(3,0,'\xCC',AT(LCY,BLK));vf(3,1,37,'\xCD',AT(LCY,BLK));vp(3,38,'\xB9',AT(LCY,BLK));
        static const char*skeys[]={"OS","Version","Author","Arch","Mode","Boot","VGA","Kernel"};
        static const char*svals[]={"Mayura OS","1.0","Susanth","i686 x86-32","32-bit Protected","GRUB Multiboot v1","80x25 Text 0xB8000","2133 lines pure C"};
        for(int i=0;i<8;i++){
            vp(4+i,0,'\xBA',AT(LCY,BLK));
            vt(4+i,2,skeys[i],AT(LGR,BLK));
            vt(4+i,12,": ",AT(DGR,BLK));
            vt(4+i,14,svals[i],AT(WHT,BLK));
            vp(4+i,38,'\xBA',AT(LCY,BLK));
        }
        vp(12,0,'\xC8',AT(LCY,BLK));vf(12,1,37,'\xCD',AT(LCY,BLK));vp(12,38,'\xBC',AT(LCY,BLK));

        /* ── Right panel: Live Stats ── */
        vp(1,41,'\xC9',AT(LGN,BLK));vf(1,42,37,'\xCD',AT(LGN,BLK));vp(1,79,'\xBB',AT(LGN,BLK));
        vp(2,41,'\xBA',AT(LGN,BLK));vc_str(2,42,37,"  LIVE STATS  ",AT(YLW,BLK));vp(2,79,'\xBA',AT(LGN,BLK));
        vp(3,41,'\xCC',AT(LGN,BLK));vf(3,42,37,'\xCD',AT(LGN,BLK));vp(3,79,'\xB9',AT(LGN,BLK));

        /* clock */
        vp(4,41,'\xBA',AT(LGN,BLK));
        vt(4,43,"Time (IST): ",AT(LGR,BLK));
        char tb[9];tb[0]='0'+h/10;tb[1]='0'+h%10;tb[2]=':';tb[3]='0'+m/10;tb[4]='0'+m%10;tb[5]=':';tb[6]='0'+s/10;tb[7]='0'+s%10;tb[8]=0;
        vt(4,55,tb,AT(LCY,BLK));
        vp(4,79,'\xBA',AT(LGN,BLK));

        /* date */
        vp(5,41,'\xBA',AT(LGN,BLK));
        vt(5,43,"Date:       ",AT(LGR,BLK));
        char db[20];db[0]='0'+day/10;db[1]='0'+day%10;db[2]=' ';kss(db+3,mon>=1&&mon<=12?MON[mon-1]:"???");db[6]=' ';db[7]='2';db[8]='0';db[9]='0'+yr/10;db[10]='0'+yr%10;db[11]=0;
        vt(5,55,db,AT(LCY,BLK));
        vp(5,79,'\xBA',AT(LGN,BLK));

        /* uptime */
        vp(6,41,'\xBA',AT(LGN,BLK));
        vt(6,43,"Uptime:     ",AT(LGR,BLK));
        char up[20];ksi(up,ticks/360000);kss(up+ksl(up),"h ");ksi(up+ksl(up),(ticks/6000)%60);kss(up+ksl(up),"m ");ksi(up+ksl(up),(ticks/100)%60);kss(up+ksl(up),"s");
        vt(6,55,up,AT(LCY,BLK));
        vp(6,79,'\xBA',AT(LGN,BLK));

        /* ticks */
        vp(7,41,'\xBA',AT(LGN,BLK));
        vt(7,43,"Ticks:      ",AT(LGR,BLK));
        char tk[12];ksi(tk,ticks);vt(7,55,tk,AT(LCY,BLK));
        vp(7,79,'\xBA',AT(LGN,BLK));

        /* CPU bar */
        vp(8,41,'\xBA',AT(LGN,BLK));
        vt(8,43,"CPU Load:   ",AT(LGR,BLK));
        int load=(int)(rrand()%30)+5;
        vt(8,55,"[",AT(DGR,BLK));
        for(int i=0;i<20;i++)vp(8,56+i,i<load/5?'\xDB':'\xB0',i<load/5?AT(LGN,BLK):AT(DGR,BLK));
        vt(8,76,"]",AT(DGR,BLK));char lp[4];ksi(lp,load);vt(8,78,lp,AT(YLW,BLK));
        vp(8,79,'\xBA',AT(LGN,BLK));

        /* RAM bar */
        vp(9,41,'\xBA',AT(LGN,BLK));
        vt(9,43,"RAM:        ",AT(LGR,BLK));
        vt(9,55,"[",AT(DGR,BLK));
        int used_fs=0;for(int i=0;i<FS_MAX;i++)if(fs[i].used)used_fs+=fs[i].size;
        int ram_pct=used_fs/1024+1;if(ram_pct>20)ram_pct=20;
        for(int i=0;i<20;i++)vp(9,56+i,i<ram_pct?'\xDB':'\xB0',i<ram_pct?AT(LBL,BLK):AT(DGR,BLK));
        vt(9,76,"]",AT(DGR,BLK));vt(9,78,"2G",AT(YLW,BLK));
        vp(9,79,'\xBA',AT(LGN,BLK));

        /* FS */
        vp(10,41,'\xBA',AT(LGN,BLK));
        vt(10,43,"FS Nodes:   ",AT(LGR,BLK));
        int fn=0;for(int i=0;i<FS_MAX;i++)if(fs[i].used)fn++;
        char fn2[16];ksi(fn2,fn);kss(fn2+ksl(fn2),"/128 used");vt(10,55,fn2,AT(LCY,BLK));
        vp(10,79,'\xBA',AT(LGN,BLK));

        vp(11,41,'\xBA',AT(LGN,BLK));vp(11,79,'\xBA',AT(LGN,BLK));
        vp(12,41,'\xC8',AT(LGN,BLK));vf(12,42,37,'\xCD',AT(LGN,BLK));vp(12,79,'\xBC',AT(LGN,BLK));

        /* ── Middle: Color palette ── */
        vp(13,0,'\xC9',AT(MAG,BLK));vf(13,1,78,'\xCD',AT(MAG,BLK));vp(13,79,'\xBB',AT(MAG,BLK));
        vp(14,0,'\xBA',AT(MAG,BLK));vc_str(14,1,78,"VGA Color Palette",AT(YLW,BLK));vp(14,79,'\xBA',AT(MAG,BLK));
        vp(15,0,'\xBA',AT(MAG,BLK));
        for(int i=0;i<16;i++){vp(15,2+i*4,'\xDB',AT(i,BLK));vp(15,3+i*4,'\xDB',AT(i,BLK));vp(15,4+i*4,' ',AT(BLK,BLK));}
        vp(15,79,'\xBA',AT(MAG,BLK));
        vp(16,0,'\xC8',AT(MAG,BLK));vf(16,1,78,'\xCD',AT(MAG,BLK));vp(16,79,'\xBC',AT(MAG,BLK));

        /* ── Bottom: Fun facts ── */
        vp(17,0,'\xC9',AT(YLW,BLK));vf(17,1,78,'\xCD',AT(YLW,BLK));vp(17,79,'\xBB',AT(YLW,BLK));
        static const char*FACTS[]={
            "This kernel has ZERO external dependencies. Not even libc.",
            "The entire OS fits in less RAM than a single browser tab.",
            "Created by Susanth. Mayura OS - pure bare metal x86.",
            "100+ shell commands, 8 games, SimpleC, chatbot - all in 2133 lines!",
            "Boots in under 2 seconds on real hardware.",
            "VGA 0xB8000: each cell is 2 bytes - char + color attribute.",
            "The PIT timer fires 100 times per second. Every tick is precious.",
        };
        static int fact_idx=0;
        vp(18,0,'\xBA',AT(YLW,BLK));
        vt(18,3,"Did you know: ",AT(LGR,BLK));
        vt(18,17,FACTS[fact_idx%7],AT(WHT,BLK));
        vp(18,79,'\xBA',AT(YLW,BLK));
        fact_idx++;
        vp(19,0,'\xC8',AT(YLW,BLK));vf(19,1,78,'\xCD',AT(YLW,BLK));vp(19,79,'\xBC',AT(YLW,BLK));

        /* Auto refresh every 2s or on key */
        uint32_t refresh=ticks+200;
        while(ticks<refresh&&kt==kh)__asm__("hlt");
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='q'||k=='Q')break;
            /* r = refresh (just redraw) */
        }
    }
    vcls();term_row=0;term_col=0;
}

/* ════════════════════════════════════════════
   WORLD CLOCK
════════════════════════════════════════════ */
static void worldclock_run(void){
    static const char*CITIES[]={"New York","Los Angeles","London","Paris","Berlin","Moscow","Dubai","Mumbai","Delhi","Kolkata","Bangkok","Singapore","Beijing","Tokyo","Sydney","Auckland","Sao Paulo","Buenos Aires","Cairo","Lagos"};
    static const int OFFSETS[]={-300,-480,0,60,60,180,240,330,330,330,420,480,480,540,600,720,-180,-180,120,60};
    static const char*REGIONS[]={"Americas","Americas","Europe","Europe","Europe","Europe","Middle East","India","India","India","SE Asia","SE Asia","East Asia","East Asia","Oceania","Oceania","S.America","S.America","Africa","Africa"};
    static const uint8_t RCOLS[]={LRD,LRD,LGN,LGN,LGN,LBL,YLW,PNK,PNK,PNK,LCY,LCY,LGR,LGR,MAG,MAG,LBL,LBL,YLW,YLW};
    while(1){
        vcls_color(AT(BLK,BLK));
        vf(0,0,VW,' ',AT(BLK,BLU));
        vc_str(0,0,VW,"\xFE WORLD CLOCK (IST = UTC+5:30) \xFE",AT(YLW,BLU));
        vf(VH-1,0,VW,' ',AT(BLK,LGR));
        vt(VH-1,2,"Q=Quit  (auto-refreshes every second)",AT(DGR,LGR));
        uint8_t hh,mm,ss;rtc_ist(&hh,&mm,&ss);
        int utc_min=(int)hh*60+(int)mm-330;
        if(utc_min<0)utc_min+=1440;
        int cols=2,per_col=10;
        for(int i=0;i<20;i++){
            int col2=i/per_col,row=i%per_col;
            int t=(utc_min+OFFSETS[i]+1440)%1440;
            int th=t/60,tm=t%60;
            int dx=col2*40,dy=2+row*2;
            char tstr[6];tstr[0]='0'+th/10;tstr[1]='0'+th%10;tstr[2]=':';tstr[3]='0'+tm/10;tstr[4]='0'+tm%10;tstr[5]=0;
            /* region tag */
            vt(dy,dx+1,REGIONS[i],AT(RCOLS[i],BLK));
            /* city */
            vt(dy,dx+12,CITIES[i],AT(WHT,BLK));
            /* time */
            vt(dy,dx+28,tstr,AT(YLW,BLK));
            /* separator */
            if(ksc(CITIES[i],"Mumbai")==0||ksc(CITIES[i],"Delhi")==0){
                /* highlight IST cities */
                vt(dy,dx+28,tstr,AT(LGN,BLK));
                vp(dy,dx+34,'*',AT(LGN,BLK));
            }
        }
        vc_str(22,0,VW,"* = IST timezone (UTC+5:30)",AT(DGR,BLK));
        /* auto-refresh */
        uint32_t rt=ticks+100;
        while(ticks<rt&&kt==kh)__asm__("hlt");
        if(kt!=kh){uint8_t k=kgetc();if(k=='q'||k=='Q')break;}
    }
    vcls();term_row=0;term_col=0;
}

/* Forward declarations */
static void todo_run(void);
static void timer_run(void);
static void mines_run(void);


/* ══════════════════════════════════════════════
   IN-OS DOCUMENTATION BROWSER
══════════════════════════════════════════════ */
#define DOCS_SECTIONS 8
static const char*DOCS_TITLES[DOCS_SECTIONS]={
    "Getting Started",
    "Shell Commands",
    "File System",
    "SimpleC Language",
    "GUI Desktop",
    "Games",
    "Music Player",
    "Keyboard Shortcuts"
};

static const char*DOCS_CONTENT[DOCS_SECTIONS][20]={
    /* Getting Started */
    {
        "MAYURA OS v1.0 - Documentation",
        "================================",
        "",
        "Mayura OS is a bare-metal x86 operating system.",
        "It runs directly on hardware with no OS beneath it.",
        "Created by Susanth.",
        "",
        "STARTUP:",
        "  Boot shows animated logo then asks GUI or CLI.",
        "  G = Launch GUI Desktop",
        "  C = Launch CLI Shell (terminal)",
        "  Auto-launches GUI after 5 seconds.",
        "",
        "IN CLI: Type commands and press Enter.",
        "  help    - show all commands",
        "  gui     - launch GUI desktop",
        "  sc      - open SimpleC REPL",
        "  docs    - open this documentation",
        "",
        NULL
    },
    /* Shell Commands */
    {
        "SHELL COMMANDS",
        "==============",
        "",
        "FILE SYSTEM:",
        "  ls / ll / la   List files",
        "  cd <dir>       Change directory",
        "  pwd            Print current path",
        "  mkdir / rmdir  Create/remove directory",
        "  touch / rm     Create/delete file",
        "  cp / mv        Copy/move files",
        "  cat / more     Read files",
        "  grep / find    Search files",
        "  wc / stat      File info",
        "  write <f> <t>  Write text to file",
        "",
        "SYSTEM:  uname  uptime  date  ps  free  df",
        "TEXT:    echo  banner  figlet  rev  upper  lower",
        "APPS:    nano  calc  clock  top  piano  music",
        "FUN:     matrix  fire  starfield  sl  hack",
        NULL
    },
    /* File System */
    {
        "FILE SYSTEM",
        "===========",
        "",
        "RAM-based: 128 nodes, 1024 bytes per file.",
        "Files are LOST on reboot.",
        "",
        "DEFAULT TREE:",
        "  /home/susanth/   Your home directory",
        "  /home/susanth/readme.txt",
        "  /home/susanth/hello.sc   SimpleC example",
        "  /etc/hostname    motd    os-release",
        "  /tmp/            Temporary files",
        "  /var/log/kern.log  Boot log",
        "  /usr/share/fortune/quotes",
        "",
        "USAGE:",
        "  write notes.txt Hello World",
        "  append notes.txt More text",
        "  cat notes.txt",
        "  nano myfile.txt  (interactive editor)",
        NULL
    },
    /* SimpleC */
    {
        "SIMPLEC LANGUAGE",
        "================",
        "Type 'sc' to open REPL. 'run file.sc' for scripts.",
        "",
        "TYPES: int  string  bool",
        "  int x = 10;",
        "  string s = \"hello\";",
        "",
        "CONTROL: if/else  while  for  break",
        "  if (x > 5) { println(x); }",
        "  for (int i=0; i<10; i++) { println(i); }",
        "",
        "MATH LIB: abs  min  max  pow  sqrt  mod  rand",
        "STRING LIB: len  upper  lower  concat  substr",
        "FILE LIB:  readfile  writefile  appendfile",
        "SYS LIB:   sleep  beep  gettime  getticks",
        "",
        "  println(\"Hello!\");",
        "  beep(440, 300);   // play A4 for 300ms",
        NULL
    },
    /* GUI */
    {
        "GUI DESKTOP",
        "===========",
        "Type 'gui' or press G at boot to enter.",
        "",
        "TABS (press Tab or F1-F4 to switch):",
        "  F1 - SysInfo  Time, date, world clocks, quotes",
        "  F2 - Apps     10 essential applications",
        "  F3 - Games    5 games",
        "  F4 - More     14 additional tools",
        "",
        "NAVIGATION:",
        "  Arrow keys  Move between icons",
        "  Enter       Open selected app",
        "  Tab         Switch to next tab",
        "  ESC         Return to CLI terminal",
        "",
        "SYSINFO AUTO-REFRESHES every second.",
        "Quotes rotate every 5 seconds (20 quotes).",
        "",
        NULL
    },
    /* Games */
    {
        "GAMES",
        "=====",
        "",
        "SNAKE   - Arrow keys to steer. Eat to grow.",
        "          Speed increases with score.",
        "",
        "TETRIS  - Arrows=move, Up=rotate, Space=drop.",
        "          Clear lines to score.",
        "",
        "PONG    - W/S keys to move paddle.",
        "          3 lives. Ball bounces off wall.",
        "",
        "2048    - Arrow keys slide tiles.",
        "          Merge same numbers. Reach 2048.",
        "",
        "MINES   - Space=reveal, F=flag bombs.",
        "          16x9 board, 20 bombs.",
        "          First click is always safe.",
        "",
        "All games: Q=quit  R=restart",
        NULL
    },
    /* Music */
    {
        "MUSIC PLAYER",
        "============",
        "Type 'music' or open from GUI Apps tab.",
        "Requires PC speaker to be enabled.",
        "",
        "TRACKS (press 1-7):",
        "  1 - Mario Theme (Game/Pop)",
        "  2 - Beethoven 5th Symphony",
        "  3 - Indian Classical Melody I",
        "  4 - Indian Classical Melody II",
        "  5 - Indian Classical Melody III",
        "  6 - Happy Birthday",
        "  7 - Rap Beat",
        "",
        "Any key stops current track.",
        "Q to quit the player.",
        "",
        "ENABLE IN VIRTUALBOX:",
        "VBoxManage setextradata VM",
        " VBoxInternal/Devices/pcspk/0/Config/Enabled 1",
        NULL
    },
    /* Shortcuts */
    {
        "KEYBOARD SHORTCUTS",
        "==================",
        "",
        "CLI SHELL:",
        "  Tab         Autocomplete command",
        "  Up/Down     History navigation",
        "  Left/Right  Move cursor in line",
        "  Home/End    Jump to start/end",
        "  PgUp        Scroll up (view history)",
        "  PgDn        Scroll down (back to bottom)",
        "  Ctrl+S      Save in nano editor",
        "  Ctrl+X      Exit nano editor",
        "",
        "GUI:",
        "  Tab / F1-F4  Switch tabs",
        "  Arrows       Navigate icons",
        "  Enter        Open app",
        "  ESC          Back to CLI",
        "",
        "SECRET:  Type 'om' for a surprise!",
        NULL
    }
};

static void docs_run(void){
    int section=0;
    int scroll=0;
    while(1){
        vcls_color(AT(BLK,BLK));
        /* Header */
        vf(0,0,VW,' ',AT(BLK,BLU));
        vt(0,1,"\xFE Mayura OS Documentation",AT(YLW,BLU));
        vt(0,50,"PgUp/Dn or Up/Dn=Scroll  Q=Quit",AT(LGR,BLU));
        /* Sidebar: section list */
        vf(1,0,VW,' ',AT(BLK,BLK));
        for(int i=0;i<DOCS_SECTIONS;i++){
            uint8_t a=(i==section)?AT(YLW,DGR):AT(LGR,BLK);
            if(i==section){vf(1,i*9,9,' ',a);}
            vt(1,i*9+1,DOCS_TITLES[i],a);
            /* truncate to 8 chars */
            if(i!=section)for(int c=i*9+1;c<i*9+9;c++)if(VGA[1*VW+c]==0)vp(1,c,' ',AT(LGR,BLK));
        }
        /* Content */
        const char**lines=DOCS_CONTENT[section];
        int line_count=0;
        while(lines[line_count])line_count++;
        int visible=VH-4;
        if(scroll>line_count-visible)scroll=line_count-visible;
        if(scroll<0)scroll=0;
        for(int r=0;r<visible;r++){
            int li=r+scroll;
            vf(2+r,0,VW,' ',AT(LGR,BLK));
            if(li<line_count&&lines[li]){
                const char*line=lines[li];
                /* Color coding */
                uint8_t a=AT(LGR,BLK);
                if(line[0]=='='||line[0]=='-')a=AT(DGR,BLK);
                else if(line[0]==' '&&line[1]==' ')a=AT(WHT,BLK);
                else if(ksl(line)>0&&line[ksl(line)-1]==':')a=AT(YLW,BLK);
                else if(line[0]>='A'&&line[0]<='Z'&&line[1]>='A'&&line[1]<='Z')a=AT(YLW,BLK);
                vt(2+r,2,line,a);
            }
        }
        /* Scrollbar */
        if(line_count>visible){
            vp(2,79,'|',AT(DGR,BLK));
            int spos=2+(int)((scroll*(VH-4))/line_count);
            vp(spos,79,'\xDB',AT(LGR,BLK));
            vp(VH-2,79,'|',AT(DGR,BLK));
        }
        /* Footer */
        vf(VH-1,0,VW,' ',AT(BLK,LGR));
        vt(VH-1,1,"1-8=Section  Up/Dn=Scroll  Q=Back",AT(DGR,LGR));
        char pg[20];kss(pg,"Line ");ksi(pg+ksl(pg),scroll+1);kss(pg+ksl(pg),"/");ksi(pg+ksl(pg),line_count);
        vt(VH-1,55,pg,AT(BLK,LGR));
        /* Input */
        uint8_t k=kgetc();
        if(k=='q'||k=='Q'||k==K_ESC)break;
        if(k>='1'&&k<='8'){section=k-'1';scroll=0;}
        if(k==K_UP||k==K_PGUP){scroll-=(k==K_PGUP?10:1);if(scroll<0)scroll=0;}
        if(k==K_DN||k==K_PGDN){scroll+=(k==K_PGDN?10:1);}
        if(k==K_LT&&section>0){section--;scroll=0;}
        if(k==K_RT&&section<DOCS_SECTIONS-1){section++;scroll=0;}
    }
    vcls();term_row=0;term_col=0;
}
/* ══════════════════════════════════════════════
   GUI WINDOW MANAGER - Tab-based desktop
   Starts with SYSINFO tab
══════════════════════════════════════════════ */

/* Famous quotes for sysinfo */
static const char*QUOTES[]={
    "The only way to do great work is to love what you do. - Steve Jobs",
    "Innovation distinguishes between a leader and a follower. - Steve Jobs",
    "Stay hungry, stay foolish. - Steve Jobs",
    "First, solve the problem. Then, write the code. - John Johnson",
    "Code is like humor. When you have to explain it, it's bad. - Cory House",
    "Any fool can write code a computer understands. Write code humans understand. - Fowler",
    "The best way to predict the future is to invent it. - Alan Kay",
    "Simplicity is the soul of efficiency. - Austin Freeman",
    "Before software can be reusable it first has to be usable. - Ralph Johnson",
    "Talk is cheap. Show me the code. - Linus Torvalds",
    "Programs must be written for people to read. - Abelson & Sussman",
    "The most disastrous thing you can ever learn is your first language. - Alan Kay",
    "Debugging is twice as hard as writing the code. - Brian Kernighan",
    "In theory, theory and practice are the same. In practice, they're not. - Yogi Berra",
    "It works on my machine. - Every Developer Ever",
    "To iterate is human, to recurse divine. - L. Peter Deutsch",
    "A ship in port is safe, but that's not what ships are built for. - Grace Hopper",
    "The computer was born to solve problems that did not exist before. - Bill Gates",
    "Measuring programming progress by lines of code is like measuring aircraft building progress by weight. - Gates",
    "There are only two hard things in CS: cache invalidation and naming things. - Phil Karlton",
    NULL
};
static int quote_idx=0;
static uint32_t quote_timer=0;

/* World times for sysinfo */
static const char*WTIME_CITIES[]={"New York","London","Dubai","Mumbai","Tokyo","Sydney"};
static const int WTIME_OFF[]={-300,0,240,330,540,600};

/* GUI state */
#define GUI_TAB_SYSINFO  0
#define GUI_TAB_APPS     1
#define GUI_TAB_GAMES    2
#define GUI_TAB_MORE     3
#define GUI_TAB_COUNT    4

static int gui_tab=GUI_TAB_SYSINFO;
static int gui_sel=0;
static int gui_more_open=0;

/* Essential 10 apps */
#define GUI_APP_N 10
static const char*GUI_APP_NAMES[GUI_APP_N]={
    "Terminal","Editor","Files","Calculator",
    "Music","Chatbot","SysInfo","WorldClock",
    "Piano","Settings"
};
static const char GUI_APP_ICONS[GUI_APP_N]={
    '\x16','\xEC','\xF4','+','\x0E','\x01','\x02','\xEB','\x06','\x2A'
};
static const uint8_t GUI_APP_COLS[GUI_APP_N]={
    GRN,BLU,BRN,CYN,PNK,LGN,RED,LBL,MAG,DGR
};

/* 5 games */
#define GUI_GAME_N 5
static const char*GUI_GAME_NAMES[GUI_GAME_N]={
    "Snake","Tetris","Pong","2048","Minesweeper"
};
static const char GUI_GAME_ICONS[GUI_GAME_N]={'\x02','\x1F','\x09','\x23','\x04'};
static const uint8_t GUI_GAME_COLS[GUI_GAME_N]={LGN,LCY,LRD,YLW,MAG};

/* More apps */
#define GUI_MORE_N 15
static const char*GUI_MORE_NAMES[GUI_MORE_N]={
    "Clock","Top","Todo","Timer","Weather","Neofetch",
    "Screensaver","Fortune","Matrix","Fire","Starfield","Hangman","TicTacToe","Docs","About"
};
static const char GUI_MORE_ICONS[GUI_MORE_N]={
    '\x0F','\x01','\xFB','\x0D','\x27','\x01',
    '\xB0','\x22','\x01','\xFE','\x2A','\x02','X','\x01'
};
static const uint8_t GUI_MORE_COLS[GUI_MORE_N]={
    MAG,RED,BLU,GRN,YLW,DGR,DGR,BRN,DGR,RED,DGR,MAG,CYN,BLU
};

/* ── Draw top menubar ───────────────────── */
static void gui_menubar(const char*tab_title){
    vf(0,0,VW,' ',AT(WHT,DGR));
    vt(0,1,"\xFE",AT(YLW,DGR));
    vt(0,3,"Mayura OS",AT(BLK,DGR));
    vp(0,13,'|',AT(LGR,DGR));
    /* tab names */
    static const char*TABS[]={"SysInfo","Apps","Games","More"};
    int tx=15;
    for(int i=0;i<GUI_TAB_COUNT;i++){
        uint8_t ta=(i==gui_tab)?AT(YLW,BLU):AT(LGR,DGR);
        if(i==gui_tab){vp(0,tx-1,' ',ta);}
        vt(0,tx,TABS[i],ta);
        if(i==gui_tab){vp(0,tx+ksl(TABS[i]),' ',ta);}
        tx+=ksl(TABS[i])+3;
    }
    vp(0,tx,'|',AT(LGR,DGR));
    /* time */
    uint8_t hh,mm,ss;rtc_ist(&hh,&mm,&ss);
    char tb[9];tb[0]='0'+hh/10;tb[1]='0'+hh%10;tb[2]=':';
    tb[3]='0'+mm/10;tb[4]='0'+mm%10;tb[5]=':';
    tb[6]='0'+ss/10;tb[7]='0'+ss%10;tb[8]=0;
    vt(0,57,"IST",AT(DGR,DGR));vt(0,61,tb,AT(YLW,DGR));
    vp(0,70,'|',AT(LGR,DGR));
    vt(0,72,"Susanth",AT(LGN,DGR));
}

/* ── Draw bottom taskbar ─────────────────── */
static void gui_taskbar(void){
    vf(VH-1,0,VW,' ',AT(BLK,LGR));
    vt(VH-1,1,"\xFE Start",AT(WHT,BLU));
    vp(VH-1,8,'|',AT(DGR,LGR));
    vt(VH-1,10,"Tab/Arrow=Navigate  Enter=Open  ESC=CLI",AT(DGR,LGR));
    vp(VH-1,51,'|',AT(DGR,LGR));
    /* show active tab */
    static const char*TABNAMES[]={"System Info","Applications","Games","More Apps"};
    vt(VH-1,53,TABNAMES[gui_tab],AT(BLK,LGR));
}

/* ── SYSINFO TAB ─────────────────────────── */
static void gui_draw_sysinfo(void){
    vcls_color(AT(BLK,BLK));
    gui_menubar("SysInfo");
    /* ── Top: Time + Date ── */
    uint8_t hh,mm,ss;rtc_ist(&hh,&mm,&ss);
    uint8_t day,mon,yr;rtc_date(&day,&mon,&yr);
    static const char*MNAMES[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    /* Big time display using existing BIG[10][5] font */
    vf(1,0,VW,' ',AT(BLK,BLK));
    /* Layout: HH:MM:SS centered, each digit 6 wide, colon 2 wide */
    /* Total = 6*6 + 2*2 = 40, start at col 20 */
    int dstart=20;
    int dpos[8]={0,6,13,19,26,32,39,45};/* H H : M M : S S */
    int dvals[8]={hh/10,hh%10,-1,mm/10,mm%10,-1,ss/10,ss%10};
    for(int d=0;d<8;d++){
        int cx=dstart+dpos[d];
        if(dvals[d]<0){
            /* colon */
            vp(2,cx,':',AT(YLW,BLK));
            vp(4,cx,':',AT(YLW,BLK));
        }else{
            /* digit using BIG font (5 rows) */
            for(int r=0;r<5;r++)
                vt(1+r,cx,BIG[dvals[d]][r],AT(LCY,BLK));
        }
    }
    /* IST label */
    vt(3,72,"IST",AT(DGR,BLK));
    /* Date */
    char dstr[32];
    kss(dstr,mon>=1&&mon<=12?MNAMES[mon-1]:"???");
    kss(dstr+ksl(dstr)," ");
    char dd[3];dd[0]='0'+day/10;dd[1]='0'+day%10;dd[2]=0;
    kss(dstr+ksl(dstr),dd);
    kss(dstr+ksl(dstr),", 20");
    char yy[3];yy[0]='0'+yr/10;yy[1]='0'+yr%10;yy[2]=0;
    kss(dstr+ksl(dstr),yy);
    vc_str(7,0,VW,dstr,AT(LGR,BLK));
    vf(8,0,VW,'\xC4',AT(DGR,BLK));
    /* ── World Times ── */
    vt(9,2,"WORLD TIMES",AT(YLW,BLK));
    int utcm=(int)hh*60+(int)mm-330;if(utcm<0)utcm+=1440;
    for(int i=0;i<6;i++){
        int t=(utcm+WTIME_OFF[i]+1440)%1440;
        int th=t/60,tm2=t%60;
        char wt[6];wt[0]='0'+th/10;wt[1]='0'+th%10;wt[2]=':';wt[3]='0'+tm2/10;wt[4]='0'+tm2%10;wt[5]=0;
        int cx=2+i*13;
        vt(10,cx,WTIME_CITIES[i],AT(LGR,BLK));
        vt(11,cx,wt,AT(LCY,BLK));
    }
    vf(12,0,VW,'\xC4',AT(DGR,BLK));
    /* ── System stats ── */
    vt(13,2,"OS:",AT(YLW,BLK));vt(13,6,"Mayura OS v1.0",AT(WHT,BLK));
    vt(13,24,"ARCH:",AT(YLW,BLK));vt(13,30,"i686 x86-32",AT(LGR,BLK));
    vt(13,44,"KERNEL:",AT(YLW,BLK));vt(13,52,"bare metal C",AT(LGR,BLK));
    vt(14,2,"UPTIME:",AT(YLW,BLK));
    char up[20];ksi(up,ticks/360000);kss(up+ksl(up),"h ");ksi(up+ksl(up),(ticks/6000)%60);kss(up+ksl(up),"m");
    vt(14,10,up,AT(LCY,BLK));
    vt(14,24,"TICKS:",AT(YLW,BLK));vi(14,31,ticks,AT(LCY,BLK));
    int fn=0;for(int i=0;i<FS_MAX;i++)if(fs[i].used)fn++;
    vt(14,44,"FILES:",AT(YLW,BLK));vi(14,51,fn,AT(LGR,BLK));vt(14,53,"/128",AT(DGR,BLK));
    vf(15,0,VW,'\xC4',AT(DGR,BLK));
    /* ── Rotating quote ── */
    vt(14,2,"\x22",AT(YLW,BLK));
    if(QUOTES[quote_idx]){
        /* wrap long quotes */
        const char*q=QUOTES[quote_idx];
        int ql=ksl(q);
        if(ql<=74){vt(14,4,q,AT(LGR,BLK));}
        else{
            /* split at space near col 74 */
            int sp=74;while(sp>0&&q[sp]!=' ')sp--;
            char line1[80];kmc(line1,q,sp);line1[sp]=0;
            vt(14,4,line1,AT(LGR,BLK));
            vt(15,4,q+sp+1,AT(DGR,BLK));
        }
    }
    /* Rotate quote every 5 seconds */
    if(ticks-quote_timer>500){
        quote_timer=ticks;
        if(QUOTES[quote_idx+1])quote_idx++;else quote_idx=0;
    }
    vf(16,0,VW,'\xC4',AT(DGR,BLK));
    /* navigation hint */
    vt(17,2,"Tab=Next Tab  Enter=Refresh  ESC=CLI",AT(DGR,BLK));
    gui_taskbar();
    cur_hide();
}

/* ── APP GRID (helper) ─────────────────── */
static void gui_draw_appgrid(
    const char**names,const char*icons,const uint8_t*cols,
    int count,int sel,int cols_per_row,int startx,int starty,int cw,int ch)
{
    for(int i=0;i<count;i++){
        int row=i/cols_per_row,col=i%cols_per_row;
        int cx=startx+col*cw,cy=starty+row*ch;
        if(cy+ch-1>=VH-1)break;
        int is_sel=(i==sel);
        uint8_t bg=cols[i];
        uint8_t ba=is_sel?AT(WHT,bg):AT(bg,DGR);
        uint8_t ia=is_sel?AT(YLW,bg):AT(WHT,DGR);
        uint8_t ta=is_sel?AT(WHT,bg):AT(LGR,DGR);
        for(int r=0;r<ch;r++)vf(cy+r,cx,cw-1,' ',ba);
        vp(cy,cx+1,icons[i],ia);
        char nm[10];int ni=0;
        while(names[i][ni]&&ni<9){nm[ni]=names[i][ni];ni++;}nm[ni]=0;
        vt(cy+1,cx+1,nm,ta);
        if(is_sel){vt(cy+2,cx+1,"\x1E Open",AT(YLW,ba>>4));}
    }
}

/* ── APPS TAB ────────────────────────────── */
static void gui_draw_apps(void){
    vcls_color(AT(BLK,BLK));
    gui_menubar("Apps");
    vf(1,0,VW,' ',AT(BLK,CYN));
    vt(1,2,"\xAF Essential Applications (10)",AT(WHT,CYN));
    vt(1,40,"Arrow keys + Enter to open",AT(DGR,CYN));
    gui_draw_appgrid(GUI_APP_NAMES,(const char*)GUI_APP_ICONS,GUI_APP_COLS,
        GUI_APP_N,gui_sel,5,1,2,15,4);
    gui_taskbar();cur_hide();
}

/* ── GAMES TAB ───────────────────────────── */
static void gui_draw_games(void){
    vcls_color(AT(BLK,BLK));
    gui_menubar("Games");
    vf(1,0,VW,' ',AT(BLK,LGN));
    vt(1,2,"\xAF Games (5)",AT(BLK,LGN));
    vt(1,25,"Arrow keys + Enter to play",AT(DGR,LGN));
    gui_draw_appgrid(GUI_GAME_NAMES,(const char*)GUI_GAME_ICONS,GUI_GAME_COLS,
        GUI_GAME_N,gui_sel,5,1,2,15,4);
    /* Game descriptions */
    if(gui_sel<GUI_GAME_N){
        static const char*GDESC[]={
            "Arrow keys to steer. Eat food to grow. Q=quit",
            "Arrows+Up=rotate, Space=drop, Q=quit",
            "W/S=paddle, 3 lives, Q=quit",
            "Arrow keys to slide tiles, reach 2048, Q=quit",
            "Space=reveal, F=flag, 16x9 board, 20 bombs"
        };
        vf(10,0,VW,' ',AT(BLK,DGR));
        vt(10,2,GDESC[gui_sel],AT(LGR,DGR));
    }
    gui_taskbar();cur_hide();
}

/* ── MORE TAB ────────────────────────────── */
static void gui_draw_more(void){
    vcls_color(AT(BLK,BLK));
    gui_menubar("More");
    vf(1,0,VW,' ',AT(BLK,MAG));
    vt(1,2,"\xAF More Applications",AT(WHT,MAG));
    vt(1,28,"Arrow keys + Enter to open",AT(DGR,MAG));
    gui_draw_appgrid(GUI_MORE_NAMES,(const char*)GUI_MORE_ICONS,GUI_MORE_COLS,
        GUI_MORE_N,gui_sel,7,1,2,11,3);
    gui_taskbar();cur_hide();
}

/* ── Launch an app by name ───────────────── */
static void gui_launch(const char*name){
    vcls();term_row=0;term_col=0;
    if(ksc(name,"Terminal")==0)return;/* exit GUI, go to CLI */
    else if(ksc(name,"Editor")==0)nano_run("untitled.txt");
    else if(ksc(name,"Files")==0)return;
    else if(ksc(name,"Calculator")==0)calc_run();
    else if(ksc(name,"Music")==0)music_run();
    else if(ksc(name,"Chatbot")==0)chat_run();
    else if(ksc(name,"SysInfo")==0)sysinfo_run();
    else if(ksc(name,"WorldClock")==0)worldclock_run();
    else if(ksc(name,"Piano")==0)piano_run();
    else if(ksc(name,"Settings")==0){term_puts("Settings: type 'help' in CLI for all options.\n");kgetc();}
    else if(ksc(name,"Snake")==0)snake_run();
    else if(ksc(name,"Tetris")==0)tetris_run();
    else if(ksc(name,"Pong")==0)pong_run();
    else if(ksc(name,"2048")==0)g2048_run();
    else if(ksc(name,"Minesweeper")==0)mines_run();
    else if(ksc(name,"Clock")==0)clock_run();
    else if(ksc(name,"Top")==0)top_run();
    else if(ksc(name,"Todo")==0)todo_run();
    else if(ksc(name,"Timer")==0)timer_run();
    else if(ksc(name,"Weather")==0){cmd_weather();kgetc();}
    else if(ksc(name,"Neofetch")==0){cmd_neofetch();kgetc();}
    else if(ksc(name,"Screensaver")==0)saver_run();
    else if(ksc(name,"Fortune")==0){cmd_fortune();kgetc();}
    else if(ksc(name,"Matrix")==0)cmd_matrix();
    else if(ksc(name,"Fire")==0)cmd_fire();
    else if(ksc(name,"Starfield")==0)cmd_starfield();
    else if(ksc(name,"Hangman")==0)hang_run();
    else if(ksc(name,"TicTacToe")==0)ttt_run();
    else if(ksc(name,"About")==0){cmd_about();kgetc();}
    else if(ksc(name,"Docs")==0)docs_run();
    vcls();term_row=0;term_col=0;
}

/* ── Main GUI loop ───────────────────────── */
static void gui_run(void){
    gui_tab=GUI_TAB_SYSINFO;
    gui_sel=0;
    quote_idx=0;
    quote_timer=ticks;
    while(1){
        /* Draw current tab */
        switch(gui_tab){
            case GUI_TAB_SYSINFO: gui_draw_sysinfo();break;
            case GUI_TAB_APPS:    gui_draw_apps();break;
            case GUI_TAB_GAMES:   gui_draw_games();break;
            case GUI_TAB_MORE:    gui_draw_more();break;
        }
        /* Input with auto-refresh for sysinfo */
        uint8_t k=0;
        if(gui_tab==GUI_TAB_SYSINFO){
            /* auto-refresh every 100 ticks (1 sec) */
            uint32_t next=ticks+100;
            while(ticks<next&&kt==kh)__asm__("hlt");
            if(kt!=kh)k=kgetc();
            else continue;/* refresh */
        }else{
            k=kgetc();
        }
        if(k==K_ESC){vcls();term_row=0;term_col=0;return;}
        /* Tab switching */
        if(k==K_TAB||k==K_RT&&gui_tab==GUI_TAB_SYSINFO){
            gui_tab=(gui_tab+1)%GUI_TAB_COUNT;
            gui_sel=0;continue;
        }
        if(k==K_F1){gui_tab=GUI_TAB_SYSINFO;gui_sel=0;continue;}
        if(k==K_F2){gui_tab=GUI_TAB_APPS;gui_sel=0;continue;}
        if(k==K_F3){gui_tab=GUI_TAB_GAMES;gui_sel=0;continue;}
        if(k==K_F4){gui_tab=GUI_TAB_MORE;gui_sel=0;continue;}
        /* Navigation within tab */
        int maxsel=0;int ncols=1;
        if(gui_tab==GUI_TAB_APPS){maxsel=GUI_APP_N-1;ncols=5;}
        else if(gui_tab==GUI_TAB_GAMES){maxsel=GUI_GAME_N-1;ncols=5;}
        else if(gui_tab==GUI_TAB_MORE){maxsel=GUI_MORE_N-1;ncols=7;}
        if(k==K_RT&&gui_sel%ncols<ncols-1&&gui_sel<maxsel)gui_sel++;
        else if(k==K_LT&&gui_sel%ncols>0)gui_sel--;
        else if(k==K_DN&&gui_sel+ncols<=maxsel)gui_sel+=ncols;
        else if(k==K_UP&&gui_sel-ncols>=0)gui_sel-=ncols;
        else if(k==K_ENT||k=='\r'){
            if(gui_tab==GUI_TAB_SYSINFO){/* refresh */continue;}
            const char*name=0;
            if(gui_tab==GUI_TAB_APPS)name=GUI_APP_NAMES[gui_sel];
            else if(gui_tab==GUI_TAB_GAMES)name=GUI_GAME_NAMES[gui_sel];
            else if(gui_tab==GUI_TAB_MORE)name=GUI_MORE_NAMES[gui_sel];
            if(name){
                gui_launch(name);
                /* if terminal was selected, exit GUI */
                if(ksc(name,"Terminal")==0)return;
            }
        }
    }
}


/* ════════════════════════════════════════════
   TODO + TIMER (simple CLI versions)
════════════════════════════════════════════ */
#define TODO_MAX 20
static char todo_items[TODO_MAX][64];
static int todo_done[TODO_MAX];
static int todo_count=0;

static void todo_run(void){
    term_puts("Todo List - commands: add <item> | done <n> | del <n> | list | q\n");
    while(1){
        term_putcolor("todo> ",AT(YLW,BLK));
        if(!line_read("todo> ",6))continue;
        if(ksc(line_buf,"q")==0)break;
        if(knc(line_buf,"add ",4)==0&&line_len>4){
            if(todo_count<TODO_MAX){kss(todo_items[todo_count],line_buf+4);todo_done[todo_count]=0;todo_count++;term_puts("Added.\n");}
            else term_puts("List full.\n");
        }else if(ksc(line_buf,"list")==0){
            for(int i=0;i<todo_count;i++){
                vp(term_row,term_col,todo_done[i]?'\xFB':'\xFE',todo_done[i]?AT(LGN,BLK):AT(LRD,BLK));term_col++;
                term_putc(' ');term_puti(i+1);term_puts(". ");
                if(todo_done[i])term_putcolor(todo_items[i],AT(DGR,BLK));
                else term_puts(todo_items[i]);
                term_nl();
            }
        }else if(knc(line_buf,"done ",5)==0&&line_len>5){
            int n=katoi(line_buf+5)-1;
            if(n>=0&&n<todo_count){todo_done[n]=1;term_puts("Done.\n");}
        }else if(knc(line_buf,"del ",4)==0&&line_len>4){
            int n=katoi(line_buf+4)-1;
            if(n>=0&&n<todo_count){for(int i=n;i<todo_count-1;i++){kss(todo_items[i],todo_items[i+1]);todo_done[i]=todo_done[i+1];}todo_count--;term_puts("Deleted.\n");}
        }else term_puts("Commands: add <item> | done <n> | del <n> | list | q\n");
    }
}

static void timer_run(void){
    term_puts("Stopwatch - S=Start/Stop L=Lap R=Reset Q=Quit\n");
    uint32_t start=0;int running=0;
    uint32_t laps[5];int nlaps=0;
    while(1){
        uint32_t elapsed=running?(ticks-start):0;
        uint32_t mins=elapsed/6000,secs=(elapsed/100)%60,hs=elapsed%100;
        /* clear line */
        vf(term_row,0,VW,' ',AT(LGR,BLK));
        term_col=0;
        char buf[32];buf[0]='0'+mins/10;buf[1]='0'+mins%10;buf[2]=':';buf[3]='0'+secs/10;buf[4]='0'+secs%10;buf[5]='.';buf[6]='0'+hs/10;buf[7]='0'+hs%10;buf[8]=0;
        term_putcolor(buf,AT(YLW,BLK));
        term_puts(running?" [RUNNING]":" [STOPPED]");
        cur_show(term_row,term_col);
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='q'||k=='Q')break;
            if(k=='s'||k=='S'){if(running)running=0;else{start=ticks;running=1;}}
            if((k=='l'||k=='L')&&running&&nlaps<5){laps[nlaps++]=ticks-start;term_nl();term_puts("Lap ");term_puti(nlaps);term_puts(": ");term_puts(buf);term_nl();}
            if(k=='r'||k=='R'){running=0;start=0;nlaps=0;term_nl();}
        }
        msleep(50);
    }
    term_nl();
}

/* ════════════════════════════════════════════
   MINESWEEPER (text mode)
════════════════════════════════════════════ */
#define MSW 16
#define MSH 9
#define MSBOMBS 20
static uint8_t msboard[MSH][MSW];
static int mscx=0,mscy=0,mslost=0,mswon=0,msfirst=1,msflags=0;
static void ms_reveal(int x,int y){if(x<0||x>=MSW||y<0||y>=MSH)return;if(msboard[y][x]&2)return;if(msboard[y][x]&4)return;msboard[y][x]|=2;if(!(msboard[y][x]>>4)){for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)ms_reveal(x+dx,y+dy);}}
static void ms_place(int fx,int fy){int p=0;while(p<MSBOMBS){int x=(int)(rrand()%MSW);int y=(int)(rrand()%MSH);if(!(msboard[y][x]&1)&&!(x==fx&&y==fy)){msboard[y][x]|=1;p++;}}for(int y=0;y<MSH;y++)for(int x=0;x<MSW;x++){if(msboard[y][x]&1)continue;int n=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int nx=x+dx,ny=y+dy;if(nx>=0&&nx<MSW&&ny>=0&&ny<MSH&&(msboard[ny][nx]&1))n++;}msboard[y][x]|=(n<<4);}}
static void mines_run(void){
    kms(msboard,0,sizeof(msboard));mscx=0;mscy=0;mslost=0;mswon=0;msfirst=1;msflags=0;
    while(1){
        vcls();
        vf(0,0,VW,' ',AT(BLK,CYN));
        vt(0,2,"MINESWEEPER",AT(BLK,CYN));vt(0,16,"Space=Open F=Flag Arrows Q=Quit R=New",AT(BLK,CYN));
        static const uint8_t MNCOL[]={DGR,BLU,GRN,RED,BLU,RED,CYN,BLK,DGR};
        int ox=8,oy=2;
        for(int y=0;y<MSH;y++)for(int x=0;x<MSW;x++){
            uint8_t c=msboard[y][x];int sel=(x==mscx&&y==mscy);
            uint8_t bg=sel?YLW:LGR;int dx=ox+x*3,dy=oy+y*2;
            if(c&2){if(c&1){vp(dy,dx,'*',AT(LRD,bg));vp(dy,dx+1,' ',AT(LRD,bg));}
                else{int nn=c>>4;vp(dy,dx,nn?'0'+nn:' ',AT(MNCOL[nn],BLK));vp(dy,dx+1,' ',AT(DGR,BLK));}}
            else if(c&4){vp(dy,dx,'\x04',AT(LRD,bg));vp(dy,dx+1,' ',AT(LRD,bg));}
            else{vp(dy,dx,'\xB2',AT(DGR,LGR));vp(dy,dx+1,' ',AT(DGR,LGR));}
        }
        vt(3,58,"Bombs: ",AT(LGR,BLK));vi(3,65,MSBOMBS,AT(WHT,BLK));
        vt(4,58,"Flags: ",AT(LGR,BLK));vi(4,65,msflags,AT(WHT,BLK));
        term_row=21;term_col=0;
        if(mslost){term_putcolor("BOOM! Hit a mine! R=New Q=Quit\n",AT(LRD,BLK));}
        else if(mswon){term_putcolor("YOU WIN! R=New Q=Quit\n",AT(LGN,BLK));}
        uint8_t k=kgetc();
        if(k=='q'||k=='Q')break;
        if(k=='r'||k=='R'){kms(msboard,0,sizeof(msboard));mscx=0;mscy=0;mslost=0;mswon=0;msfirst=1;msflags=0;continue;}
        if(mslost||mswon)continue;
        if(k==K_UP&&mscy>0)mscy--;else if(k==K_DN&&mscy<MSH-1)mscy++;
        if(k==K_LT&&mscx>0)mscx--;else if(k==K_RT&&mscx<MSW-1)mscx++;
        if(k==' '){if(msfirst){ms_place(mscx,mscy);msfirst=0;}if(!(msboard[mscy][mscx]&4)){if(msboard[mscy][mscx]&1){msboard[mscy][mscx]|=2;mslost=1;}else{ms_reveal(mscx,mscy);int ok=1;for(int y=0;y<MSH;y++)for(int x=0;x<MSW;x++)if(!(msboard[y][x]&1)&&!(msboard[y][x]&2))ok=0;if(ok)mswon=1;}}}
        if(k=='f'||k=='F'){if(!(msboard[mscy][mscx]&2)){msboard[mscy][mscx]^=4;msflags+=(msboard[mscy][mscx]&4)?1:-1;}}
    }
    vcls();term_row=0;term_col=0;
}

/* ════════════════════════════════════════════
   MAIN SHELL DISPATCH
════════════════════════════════════════════ */
static void shell_prompt(void){
    char path[64];fs_cwd_path(path);
    /* shorten path: show only last 2 components to keep prompt short */
    char short_path[24];
    int pl=ksl(path);
    if(pl<=20){
        kss(short_path,path);
    } else {
        /* find second-to-last slash */
        int slashes=0,last2=0;
        for(int i=pl-1;i>=0;i--){if(path[i]=='/'){slashes++;if(slashes==2){last2=i;break;}}}
        short_path[0]='.';short_path[1]='.';
        int j=2;for(int i=last2;path[i]&&j<23;i++)short_path[j++]=path[i];
        short_path[j]=0;
    }
    if(term_col>0)term_nl();
    term_attr=LGR|(BLK<<4);
    term_putcolor("susanth",AT(LGN,BLK));
    term_putcolor("@mayura",AT(DGR,BLK));
    term_putc(':');
    term_putcolor(short_path,AT(LBL,BLK));
    term_putcolor(" $ ",AT(WHT,BLK));
    term_attr=LGR|(BLK<<4);
}

static void exec_cmd(void){
    parse_cmd();
    if(cmd_argc==0)return;
    const char*cmd=cmd_argv[0];
    const char*a1=(cmd_argc>1)?cmd_argv[1]:"";
    const char*a2=(cmd_argc>2)?cmd_argv[2]:"";

    /* ── File system ── */
    if(ksc(cmd,"ls")==0){
        int fl=0;for(int i=1;i<cmd_argc;i++)if(ksc(cmd_argv[i],"-l")==0)fl=1;
        cmd_ls(fl);
    }
    else if(ksc(cmd,"ll")==0)cmd_ls(1);
    else if(ksc(cmd,"la")==0)cmd_ls(0);
    else if(ksc(cmd,"cd")==0){
        if(!ksl(a1)||ksc(a1,"~")==0||ksc(a1,"/home/susanth")==0){
            /* go to home */
            int home=fs_find("home",0);if(home>=0){int sus=fs_find("susanth",home);if(sus>=0)fs_cwd=sus;}
        }else if(ksc(a1,"/")==0)fs_cwd=0;
        else if(ksc(a1,"..")==0){if(fs[fs_cwd].parent>=0)fs_cwd=fs[fs_cwd].parent;}
        else{int d=fs_find(a1,fs_cwd);if(d>=0&&fs[d].is_dir)fs_cwd=d;else{term_puts("cd: ");term_puts(a1);term_puts(": Not a directory\n");}}
    }
    else if(ksc(cmd,"pwd")==0){char p[64];fs_cwd_path(p);term_puts(p);term_nl();}
    else if(ksc(cmd,"mkdir")==0){if(!ksl(a1)){term_puts("mkdir: missing operand\n");}else if(fs_find(a1,fs_cwd)>=0)term_puts("mkdir: already exists\n");else if(fs_mkdir(a1,fs_cwd)<0)term_puts("mkdir: no space\n");}
    else if(ksc(cmd,"rmdir")==0){int n=fs_find(a1,fs_cwd);if(n<0||!fs[n].is_dir)term_puts("rmdir: not a directory\n");else{fs[n].used=0;}}
    else if(ksc(cmd,"touch")==0){if(!ksl(a1))term_puts("touch: missing\n");else if(fs_find(a1,fs_cwd)<0)fs_mkfile(a1,fs_cwd,NULL);}
    else if(ksc(cmd,"rm")==0){int n=fs_find(a1,fs_cwd);if(n<0)term_puts("rm: not found\n");else{fs[n].used=0;}}
    else if(ksc(cmd,"cp")==0){int sn=fs_find(a1,fs_cwd);if(sn<0){term_puts("cp: not found\n");}else{int dn=fs_alloc();if(dn<0)term_puts("cp: no space\n");else{kmc(&fs[dn],&fs[sn],sizeof(FSNode));kss(fs[dn].name,ksl(a2)?a2:a1);fs[dn].parent=fs_cwd;}}}
    else if(ksc(cmd,"mv")==0){int n=fs_find(a1,fs_cwd);if(n<0)term_puts("mv: not found\n");else kss(fs[n].name,a2);}
    else if(ksc(cmd,"cat")==0){if(ksl(a1))cmd_cat(a1);else term_puts("cat: missing operand\n");}
    else if(ksc(cmd,"head")==0){int n=fs_find(a1,fs_cwd);if(n<0){term_puts("head: not found\n");}else{char line[80];int li=0,fi=0,lc=0;while(fi<=fs[n].size&&lc<10){if(fi==fs[n].size||fs[n].data[fi]=='\n'){line[li]=0;term_puts(line);term_nl();li=0;lc++;}else if(li<79)line[li++]=fs[n].data[fi];fi++;}}}
    else if(ksc(cmd,"tail")==0){int n=fs_find(a1,fs_cwd);if(n<0){term_puts("tail: not found\n");}else{int nl=0;for(int i=0;i<fs[n].size;i++)if(fs[n].data[i]=='\n')nl++;int skip=nl>10?nl-10:0,sc2=0;char line[80];int li=0,fi=0;while(fi<=fs[n].size){if(fi==fs[n].size||fs[n].data[fi]=='\n'){line[li]=0;if(sc2>=skip){term_puts(line);term_nl();}sc2++;li=0;}else if(li<79)line[li++]=fs[n].data[fi];fi++;}}}
    else if(ksc(cmd,"more")==0){int n=fs_find(a1,fs_cwd);if(n<0){term_puts("more: not found\n");}else{int lines=0;for(int i=0;i<fs[n].size;i++){term_putc(fs[n].data[i]);if(fs[n].data[i]=='\n'){lines++;if(lines%20==0){term_putcolor("--More-- (any key)",AT(BLK,LGR));kgetc();vf(term_row,0,VW,' ',AT(LGR,BLK));term_col=0;}}}term_nl();}}
    else if(ksc(cmd,"wc")==0){if(ksl(a1))cmd_wc(a1);else term_puts("wc: missing\n");}
    else if(ksc(cmd,"grep")==0){if(cmd_argc>=3)cmd_grep(a1,a2);else term_puts("grep: usage: grep <pattern> <file>\n");}
    else if(ksc(cmd,"find")==0){cmd_find_r(0,ksl(a1)?a1:"","");}
    else if(ksc(cmd,"tree")==0){term_puts("/\n");cmd_tree_r(0,1);}
    else if(ksc(cmd,"du")==0){int total=0;for(int i=0;i<FS_MAX;i++)if(fs[i].used&&!fs[i].is_dir&&fs[i].parent==fs_cwd)total+=fs[i].size;term_puti(total);term_puts(" bytes\n");}
    else if(ksc(cmd,"stat")==0){int n=fs_find(a1,fs_cwd);if(n<0){term_puts("stat: not found\n");}else{term_puts("File: ");term_puts(fs[n].name);term_nl();term_puts("Type: ");term_puts(fs[n].is_dir?"directory":"regular file");term_nl();term_puts("Size: ");term_puti(fs[n].size);term_puts(" bytes\n");}}
    else if(ksc(cmd,"file")==0){int n=fs_find(a1,fs_cwd);if(n<0){term_puts("file: not found\n");}else{term_puts(a1);term_puts(": ");term_puts(fs[n].is_dir?"directory":"ASCII text");term_nl();}}
    else if(ksc(cmd,"write")==0||ksc(cmd,"tee")==0){
        if(cmd_argc<3){term_puts("usage: write <file> <content>\n");}
        else{int n=fs_find(a1,fs_cwd);if(n<0){n=fs_alloc();if(n<0){term_puts("no space\n");}else{fs[n].used=1;kss(fs[n].name,a1);fs[n].parent=fs_cwd;}}
        if(n>=0){int cl=ksl(a2);if(cl>=FS_DSIZ-1)cl=FS_DSIZ-2;kmc(fs[n].data,a2,cl);fs[n].data[cl]='\n';fs[n].size=cl+1;}}
    }
    else if(ksc(cmd,"append")==0){
        if(cmd_argc<3)term_puts("usage: append <file> <content>\n");
        else{int n=fs_find(a1,fs_cwd);if(n<0)term_puts("append: not found\n");else{int cl=ksl(a2);int avail=FS_DSIZ-1-fs[n].size;if(cl>avail)cl=avail;kmc(fs[n].data+fs[n].size,a2,cl);fs[n].data[fs[n].size+cl]='\n';fs[n].size+=cl+1;}}
    }
    else if(ksc(cmd,"truncate")==0){int n=fs_find(a1,fs_cwd);if(n>=0){kms(fs[n].data,0,FS_DSIZ);fs[n].size=0;}}
    else if(ksc(cmd,"ln")==0){term_puts("ln: soft links not supported (RAM FS)\n");}

    /* ── Text ── */
    else if(ksc(cmd,"echo")==0){for(int i=1;i<cmd_argc;i++){if(i>1)term_putc(' ');term_puts(cmd_argv[i]);}term_nl();}
    else if(ksc(cmd,"printf")==0){for(int i=1;i<cmd_argc;i++){term_puts(cmd_argv[i]);term_putc(' ');}term_nl();}
    else if(ksc(cmd,"clear")==0||ksc(cmd,"reset")==0)term_clear();
    else if(ksc(cmd,"banner")==0){char big[4*4*65];kms(big,0,sizeof(big));for(int r=0;r<3;r++){for(int i=1;i<cmd_argc;i++){char c=cmd_argv[i][0];if(r==0){term_putc(' ');term_putc(c);term_putc(c);term_putc(' ');}else if(r==1){term_putc(c);term_puts("  ");term_putc(c);}else{term_putc(' ');term_putc(c);term_putc(c);term_putc(' ');}term_putc(' ');}term_nl();}(void)big;}
    else if(ksc(cmd,"figlet")==0){if(ksl(a1))cmd_figlet(a1);else term_puts("figlet: missing text\n");}
    else if(ksc(cmd,"rev")==0){char buf[LINE_MAX];kss(buf,a1);int l=ksl(buf);for(int i=0;i<l/2;i++){char t=buf[i];buf[i]=buf[l-1-i];buf[l-1-i]=t;}term_puts(buf);term_nl();}
    else if(ksc(cmd,"upper")==0){char buf[LINE_MAX];kss(buf,a1);kupper(buf);term_puts(buf);term_nl();}
    else if(ksc(cmd,"lower")==0){char buf[LINE_MAX];kss(buf,a1);klower(buf);term_puts(buf);term_nl();}
    else if(ksc(cmd,"rot13")==0){for(int i=0;a1[i];i++){char c=a1[i];if(c>='a'&&c<='z')c=(c-'a'+13)%26+'a';else if(c>='A'&&c<='Z')c=(c-'A'+13)%26+'A';term_putc(c);}term_nl();}
    else if(ksc(cmd,"seq")==0){int s=1,e=katoi(a1);if(cmd_argc>2){s=katoi(a1);e=katoi(a2);}for(int i=s;i<=e;i++){term_puti(i);term_nl();}}
    else if(ksc(cmd,"yes")==0){term_puts("y\ny\ny\n(use Ctrl+C to stop in a real system)\n");}

    /* ── System ── */
    else if(ksc(cmd,"uname")==0){
        int a=(ksc(a1,"-a")==0);
        term_puts("MayuraOS");if(a){term_puts(" mayura 1.0 #1 i686 GNU/Mayura");}term_nl();
    }
    else if(ksc(cmd,"uptime")==0){
        term_puts("up ");term_puti(ticks/360000);term_puts("h ");term_puti((ticks/6000)%60);term_puts("m ");term_puti((ticks/100)%60);term_puts("s, 1 user, load: 0.01 0.01 0.00\n");
    }
    else if(ksc(cmd,"date")==0||ksc(cmd,"time")==0){
        uint8_t h,m,s;rtc_ist(&h,&m,&s);uint8_t day,mon,yr;rtc_date(&day,&mon,&yr);
        static const char*MONTHS[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        term_puts(MONTHS[(mon>0&&mon<=12)?mon-1:0]);term_putc(' ');
        term_puti(day);term_puts(" 20");
        if(yr<10)term_putc('0');term_puti(yr);term_puts("  ");
        char tb[9];tb[0]='0'+h/10;tb[1]='0'+h%10;tb[2]=':';tb[3]='0'+m/10;tb[4]='0'+m%10;tb[5]=':';tb[6]='0'+s/10;tb[7]='0'+s%10;tb[8]=0;
        term_puts(tb);term_puts(" IST\n");
    }
    else if(ksc(cmd,"whoami")==0)term_puts("susanth\n");
    else if(ksc(cmd,"id")==0)term_puts("uid=0(susanth) gid=0(root) groups=0(root)\n");
    else if(ksc(cmd,"groups")==0)term_puts("root wheel sudo\n");
    else if(ksc(cmd,"hostname")==0){int n=fs_find("etc",0);if(n>=0){int h=fs_find("hostname",n);if(h>=0){term_puts(fs[h].data);return;}}term_puts("mayura\n");}
    else if(ksc(cmd,"ps")==0){
        term_putcolor("  PID TTY      STAT CMD\n",AT(YLW,BLK));
        term_puts("    1 tty1     S    mayura-kernel\n");
        term_puts("    2 tty1     S    mayura-sh\n");
        term_puts("    3 tty1     R    ps\n");
    }
    else if(ksc(cmd,"kill")==0)term_puts("kill: No process to kill in this kernel.\n");
    else if(ksc(cmd,"env")==0){
        term_puts("PATH=/bin:/usr/bin\n");
        term_puts("HOME=/home/susanth\n");
        term_puts("USER=susanth\n");
        term_puts("SHELL=/bin/mayura-sh\n");
        term_puts("TERM=vt100\n");
        term_puts("OS=MayuraOS\n");
    }
    else if(ksc(cmd,"history")==0){int start=hist_count>HIST_MAX?hist_count-HIST_MAX:0;for(int i=start;i<hist_count;i++){term_puts("  ");term_puti(i+1);term_puts("  ");term_puts(hist[i%HIST_MAX]);term_nl();}}
    else if(ksc(cmd,"alias")==0)term_puts("ll='ls -l'\nla='ls -a'\n");
    else if(ksc(cmd,"export")==0){term_puts("export: ");term_puts(a1);term_puts(" (RAM only)\n");}
    else if(ksc(cmd,"free")==0){term_puts("              total    used    free\nMem:        2097152       0 2097152\nSwap:             0       0       0\n");}
    else if(ksc(cmd,"df")==0){
        term_puts("Filesystem      Size  Used Avail Use% Mounted on\n");
        int used=0;for(int i=0;i<FS_MAX;i++)if(fs[i].used)used+=fs[i].size;
        term_puts("ramfs           64K  ");term_puti(used/1024+1);term_puts("K  ");term_puti(64-(used/1024+1));term_puts("K  /\n");
    }
    else if(ksc(cmd,"lscpu")==0){term_puts("Architecture:  i686\nCPU op-modes:  32-bit\nVendor:        Intel\nModel name:    bare metal x86\nCPU(s):        1\n");}
    else if(ksc(cmd,"lsmod")==0)term_puts("Module                  Size  Used by\n(no modules - bare metal kernel)\n");
    else if(ksc(cmd,"dmesg")==0){cmd_cat("kern.log");}
    else if(ksc(cmd,"reboot")==0||ksc(cmd,"shutdown")==0){term_puts("Goodbye...\n");msleep(500);outb(0x64,0xFE);}
    else if(ksc(cmd,"halt")==0){term_puts("Halting...\n");__asm__("cli");for(;;)__asm__("hlt");}
    else if(ksc(cmd,"version")==0||ksc(cmd,"about")==0)cmd_about();

    /* ── Apps ── */
    else if(ksc(cmd,"nano")==0)nano_run(ksl(a1)?a1:"untitled.txt");
    else if(ksc(cmd,"calc")==0)calc_run();
    else if(ksc(cmd,"clock")==0)clock_run();
    else if(ksc(cmd,"top")==0)top_run();
    else if(ksc(cmd,"piano")==0)piano_run();
    else if(ksc(cmd,"fortune")==0)cmd_fortune();
    else if(ksc(cmd,"cowsay")==0)cmd_cowsay(ksl(a1)?a1:"Moo!");
    else if(ksc(cmd,"neofetch")==0)cmd_neofetch();
    else if(ksc(cmd,"weather")==0)cmd_weather();
    else if(ksc(cmd,"todo")==0)todo_run();
    else if(ksc(cmd,"timer")==0)timer_run();
    else if(ksc(cmd,"sc")==0)sc_repl();
    else if(ksc(cmd,"run")==0){if(ksl(a1))sc_run_file(a1);else term_puts("run: specify a .sc file\n");}
    else if(ksc(cmd,"compile")==0){term_puts("compile: SimpleC is interpreted, use 'run' instead.\n");}
    else if(ksc(cmd,"gui")==0)gui_run();
    else if(ksc(cmd,"music")==0)music_run();
    else if(ksc(cmd,"chat")==0)chat_run();
    else if(ksc(cmd,"saver")==0||ksc(cmd,"screensaver")==0)saver_run();
    else if(ksc(cmd,"sysinfo")==0)sysinfo_run();
    else if(ksc(cmd,"worldclock")==0||ksc(cmd,"wclock")==0)worldclock_run();

    /* ── Games ── */
    else if(ksc(cmd,"snake")==0)snake_run();
    else if(ksc(cmd,"tetris")==0)tetris_run();
    else if(ksc(cmd,"pong")==0)pong_run();
    else if(ksc(cmd,"2048")==0)g2048_run();
    else if(ksc(cmd,"hang")==0)hang_run();
    else if(ksc(cmd,"ttt")==0)ttt_run();
    else if(ksc(cmd,"guess")==0)guess_run();
    else if(ksc(cmd,"mines")==0)mines_run();

    /* ── Animations ── */
    else if(ksc(cmd,"matrix")==0)cmd_matrix();
    else if(ksc(cmd,"fire")==0)cmd_fire();
    else if(ksc(cmd,"starfield")==0)cmd_starfield();
    else if(ksc(cmd,"sl")==0)cmd_sl();
    else if(ksc(cmd,"hack")==0)cmd_hack();

    /* ── Easter eggs ── */
    else if(ksc(cmd,"om")==0)cmd_om();/* secret */
    else if(ksc(cmd,"konami")==0)cmd_konami();
    else if(ksc(cmd,"sudo")==0){
        if(ksc(a1,"make")==0&&ksc(a2,"me")==0)term_puts("okay\n");
        else if(ksc(a1,"rm")==0)term_puts("Nice try.\n");
        else{term_puts("susanth is not in the sudoers file. This incident will be reported.\n");}
    }
    else if(ksc(cmd,"git")==0)term_puts("error: No git. No mercy.\n");
    else if(ksc(cmd,"python")==0||ksc(cmd,"python3")==0)term_puts("No Python here. Use SimpleC  (type 'sc')\n");
    else if(ksc(cmd,"vim")==0||ksc(cmd,"vi")==0){term_puts("Welcome to vim.\n");msleep(2000);term_puts("\nJust kidding. Opening nano...\n");msleep(500);nano_run(ksl(a1)?a1:"untitled.txt");}
    else if(ksc(cmd,"exit")==0||ksc(cmd,"logout")==0)term_puts("You can't leave. This is bare metal.\n");
    else if(ksc(cmd,"credits")==0)cmd_about();
    else if(ksc(cmd,"morse")==0){
        /* morse translator */
        static const char*MORSE[26]={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
        const char*s=a1;while(*s){char c=*s++;if(c>='a'&&c<='z')c-=32;if(c>='A'&&c<='Z'){term_puts(MORSE[c-'A']);term_putc(' ');}else if(c==' ')term_puts("/ ");}term_nl();
    }
    else if(ksc(cmd,"type")==0){
        /* typing speed test */
        const char*TEXT="the quick brown fox jumps over the lazy dog";
        term_puts("Type: ");term_puts(TEXT);term_nl();
        term_putcolor("Go: ",AT(LGN,BLK));
        uint32_t tstart=ticks;
        if(!line_read("Go: ",4))return;
        uint32_t telapsed=ticks-tstart;
        int correct=(ksc(line_buf,TEXT)==0);
        if(correct&&telapsed>0){int wpm=(int)(9*6000/telapsed);term_puts("Correct! WPM: ");term_puti(wpm);term_nl();}
        else if(!correct)term_puts("Incorrect. Try again.\n");
    }
    else if(ksc(cmd,"quiz")==0){
        static const char*Q[]={"What does CPU stand for?","Who made Linux?","What is 2^8?","What lang is Linux written in?"};
        static const char*A[]={"central processing unit","linus torvalds","256","c"};
        term_putcolor("General Knowledge Quiz\n",AT(YLW,BLK));
        int score=0,i2=0;for(i2=0;i2<4;i2++){int i=i2;term_puts(Q[i]);term_nl();term_putcolor("A: ",AT(LGN,BLK));if(!line_read("A: ",3)){i++;continue;}char lb2[LINE_MAX];kss(lb2,line_buf);klower(lb2);if(ksc(lb2,A[i])==0){term_putcolor("Correct!\n",AT(LGN,BLK));score++;}else{term_putcolor("Wrong. Answer: ",AT(LRD,BLK));term_puts(A[i]);term_nl();}}
        term_puts("Score: ");term_puti(score);term_puts("/4\n");
    }
    else if(ksc(cmd,"math")==0){
        term_puts("Math Drill - 10 questions (q to quit)\n");
        int score=0;
        for(int i=0;i<10;i++){
            rng^=ticks;int a=1+(int)(rrand()%20),b=1+(int)(rrand()%20),op=(int)(rrand()%3);
            static const char*OPS[]={"plus","minus","times"};
            term_puti(a);term_putc(' ');term_puts(OPS[op]);term_puts(" ");term_puti(b);term_puts(" = ");
            if(!line_read("= ",2))continue;
            if(ksc(line_buf,"q")==0)break;
            int ans=katoi(line_buf);
            int correct=op==0?a+b:op==1?a-b:a*b;
            if(ans==correct){term_putcolor("Correct!\n",AT(LGN,BLK));score++;}
            else{term_putcolor("Wrong. ",AT(LRD,BLK));term_puti(correct);term_nl();}
        }
        term_puts("Score: ");term_puti(score);term_puts("/10\n");
    }

    /* ── Help ── */
    else if(ksc(cmd,"help")==0||ksc(cmd,"man")==0){
        term_putcolor("Mayura OS Shell Commands\n",AT(YLW,BLK));
        term_puts("━━━━━━━━━━━━━━━━━━━━━━━━\n");
        term_putcolor("Files:   ",AT(LGN,BLK));term_puts("ls ll cd pwd mkdir rmdir touch rm cp mv cat head tail more wc grep find du stat file tree write append truncate\n");
        term_putcolor("Text:    ",AT(LGN,BLK));term_puts("echo printf clear banner figlet rev upper lower rot13 seq\n");
        term_putcolor("System:  ",AT(LGN,BLK));term_puts("uname uptime date whoami id hostname ps env history free df lscpu dmesg reboot halt\n");
        term_putcolor("Apps:    ",AT(LGN,BLK));term_puts("nano calc clock top piano fortune cowsay neofetch weather todo timer morse type quiz math\n");
        term_putcolor("Code:    ",AT(LGN,BLK));term_puts("sc (SimpleC REPL)  run <file.sc>\n");
        term_putcolor("Games:   ",AT(LGN,BLK));term_puts("snake tetris pong 2048 hang ttt guess mines\n");
        term_putcolor("Fun:     ",AT(LGN,BLK));term_puts("matrix fire starfield sl hack konami\n");
        term_putcolor("GUI:     ",AT(LGN,BLK));term_puts("gui\n");
        term_puts("Type 'man <cmd>' for details. Some secrets are hidden...\n");
    }

    /* ── Unknown ── */
    else{
        term_puts(cmd);term_puts(": command not found. Type 'help'\n");
    }
}

/* ════════════════════════════════════════════
   BOOT SEQUENCE
════════════════════════════════════════════ */
/* ══════════════════════════════════════════════
   ANIMATED BOOT LOGO + CHOOSER
══════════════════════════════════════════════ */
static void boot_logo(void){
    vcls_color(AT(BLK,BLK));
    /* Mayura (Peacock) ASCII art - animates in */
    static const char*PEACOCK[]={
        "                    .  *  .                    ",
        "                 *  .  *  .  *                 ",
        "              .  *  .  *  .  *  .              ",
        "           *  .  *     *     *  .  *           ",
        "        .  *  .     *  *  *     .  *  .        ",
        "           *     *  *  *  *  *     *           ",
        "              .     *  *  *     .              ",
        "                 *  *  *  *  *                 ",
        "              .  *  *  *  *  *  .              ",
        "                    *  *  *                    ",
        "                    *  *  *                    ",
        "                   *  ***  *                   ",
        "                   *  ***  *                   ",
        "                     *  *                     ",
        "                     ****                     ",
        "                    ******                    ",
        NULL
    };
    static const uint8_t PCOLS[]={LGN,LCY,YLW,LGN,MAG,LCY,YLW,LGN,MAG,LCY,LGN,YLW,LCY,LGN,LGR,DGR};
    /* Animate each line appearing */
    for(int i=0;PEACOCK[i];i++){
        int r=3+i;
        int len=ksl(PEACOCK[i]);
        int cx=(VW-len)/2;
        /* Character by character reveal */
        for(int j=0;j<len;j++){
            vp(r,cx+j,PEACOCK[i][j],AT(PCOLS[i],BLK));
            if(j%3==0)msleep(8);
        }
        msleep(40);
    }
    /* Title animates in */
    msleep(200);
    static const char*TITLE="  M A Y U R A   O S  ";
    static const char*SUB="    v 1 . 0  —  B a r e  M e t a l  x 8 6    ";
    int tc=(VW-ksl(TITLE))/2;
    int sc2=(VW-ksl(SUB))/2;
    /* Type title char by char */
    for(int i=0;TITLE[i];i++){
        vp(20,tc+i,TITLE[i],AT(YLW,BLK));
        msleep(40);
    }
    msleep(200);
    for(int i=0;SUB[i];i++){
        vp(21,sc2+i,SUB[i],AT(LGR,BLK));
        msleep(20);
    }
    msleep(300);
    /* Boot log at bottom, scrolling fast */
    static const char*MSGS[]={
        "Memory subsystem: 2048MB RAM",
        "Interrupt descriptor table: OK",
        "PIC remapped: IRQ0-7 > 0x20",
        "PIT timer: 100Hz",
        "PS/2 keyboard driver: OK",
        "VGA text mode: 80x25 @ 0xB8000",
        "RAM filesystem: 128 nodes",
        "SimpleC interpreter v1.0: loaded",
        "Desktop environment: ready",
        "Boot complete",
        NULL
    };
    for(int i=0;MSGS[i];i++){
        int mr=VH-1;
        vf(mr,0,VW,' ',AT(BLK,BLK));
        vt(mr,2,"[ ",AT(DGR,BLK));
        vt(mr,4,AT(LGN,BLK)==AT(LGN,BLK)?"OK":"OK",AT(LGN,BLK));
        vt(mr,6," ] ",AT(DGR,BLK));
        vt(mr,10,MSGS[i],AT(LGR,BLK));
        msleep(120);
    }
    msleep(800);
}

/* Boot mode chooser */
#define BOOT_GUI 0
#define BOOT_CLI 1
static int boot_mode=BOOT_GUI;

static void boot_chooser(void){
    vcls_color(AT(BLK,BLK));
    /* Simple clean chooser */
    int r=8;
    vf(r,10,60,' ',AT(BLK,BLU));
    vc_str(r,10,60," Mayura OS v1.0 ",AT(YLW,BLU));
    vf(r+1,10,60,' ',AT(BLK,BLU));
    vc_str(r+1,10,60,"Select startup mode:",AT(LGR,BLU));
    vf(r+2,10,60,' ',AT(BLK,BLU));
    /* Options */
    vf(r+4,15,50,' ',AT(BLK,GRN));
    vc_str(r+4,15,50,"  [G]  GUI Desktop  ",AT(WHT,GRN));
    vf(r+5,15,50,' ',AT(BLK,GRN));
    vc_str(r+5,15,50,"  Launch graphical desktop with apps",AT(BLK,GRN));
    vf(r+7,15,50,' ',AT(BLK,DGR));
    vc_str(r+7,15,50,"  [C]  CLI Terminal  ",AT(WHT,DGR));
    vf(r+8,15,50,' ',AT(BLK,DGR));
    vc_str(r+8,15,50,"  Command line shell with 100+ commands",AT(LGR,DGR));
    /* Timer */
    vf(r+11,10,60,' ',AT(BLK,BLK));
    vc_str(r+11,10,60,"Auto-starts GUI in 5 seconds...",AT(DGR,BLK));
    uint32_t deadline=ticks+500;/* 5 seconds */
    int chosen=BOOT_GUI;
    while(ticks<deadline){
        /* Countdown */
        int remaining=(int)((deadline-ticks)/100)+1;
        char cstr[4];ksi(cstr,remaining);
        vf(r+11,10,60,' ',AT(BLK,BLK));
        char msg[64];kss(msg,"Auto-starts GUI in ");ksi(msg+ksl(msg),remaining);kss(msg+ksl(msg)," seconds...");
        vc_str(r+11,10,60,msg,AT(DGR,BLK));
        if(kt!=kh){
            uint8_t k=kgetc();
            if(k=='g'||k=='G'){chosen=BOOT_GUI;break;}
            if(k=='c'||k=='C'||k==K_ESC){chosen=BOOT_CLI;break;}
        }
        __asm__("hlt");
    }
    boot_mode=chosen;
    vcls_color(AT(BLK,BLK));
}

static void boot_sequence(void){
    boot_logo();/* 7 second animated logo */
    boot_chooser();/* GUI or CLI choice */
}

/* ════════════════════════════════════════════
   KERNEL MAIN
════════════════════════════════════════════ */
void kernel_main(uint32_t magic,void*mbi){
    (void)magic;(void)mbi;
    vcls();
    /* IDT */
    kms(IDT,0,sizeof(IDT));
    for(int i=0;i<256;i++)idt_set(i,(uint32_t)isr_noerr);
    idt_set(8,(uint32_t)isr_err);idt_set(10,(uint32_t)isr_err);
    idt_set(11,(uint32_t)isr_err);idt_set(12,(uint32_t)isr_err);
    idt_set(13,(uint32_t)isr_err);idt_set(14,(uint32_t)isr_err);
    idt_set(17,(uint32_t)isr_err);
    for(int i=32;i<48;i++)idt_set(i,(uint32_t)isr_irq);
    idt_set(0x20,(uint32_t)isr_timer);
    idt_set(0x21,(uint32_t)isr_keyboard);
    IDTP.lim=sizeof(IDT)-1;IDTP.base=(uint32_t)IDT;
    __asm__ volatile("lidt %0"::"m"(IDTP));
    /* PIC + PIT */
    pic_init();pit_init();
    __asm__ volatile("sti");
    /* FS */
    fs_init();
    rng^=ticks;
    /* Boot */
    boot_sequence();
    /* Launch based on user choice */
    if(boot_mode==0){
        gui_run();
        /* if user exits GUI, fall through to CLI */
    }
    /* Shell loop */
    while(1){
        shell_prompt();
        /* prompt = "susanth@mayura:/path $ "
           cap prompt_len so line_redraw never goes past VW */
        {char _p[64];fs_cwd_path(_p);
        int _plen=7+7+1+ksl(_p)+3;
        if(_plen>=VW-10)_plen=VW-10;
        line_read("$ ",_plen);}
        if(line_len>0)exec_cmd();
    }
}
