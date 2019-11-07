#!/usr/bin/python3
# -*- coding: utf-8 -*-

import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor
from PyQt5.QtCore import Qt,QDir,QBasicTimer
import socket
import re
import os
import ctypes
import select
import inspect
import time
import threading


bufferSize = 4096
# create the socket for control
sock_ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host_ip=''
host_port=0
username=''
email_address=''
file_mode=-1
conn_ip=''
conn_port=0
my_conn_ip=''
my_conn_port=0
ifRest=0
list_cur_path=''
restPos=0

# record filepath on the [server]
cur_download_file=''
cur_download_size=0.0

# default
downloadPath="/home/wangyingqi/桌面"
#indicate th state of downloading or uploading
isdownload=0
isupload=0
# record current size of sending  when uploading
storsize=0
# record full size
storAllSize=10

# to stop a thread
def _async_raise(tid, exctype):
    """raises the exception, performs cleanup if needed"""
    tid = ctypes.c_long(tid)
    if not inspect.isclass(exctype):
        exctype = type(exctype)
    res = ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, ctypes.py_object(exctype))
    if res == 0:
        raise ValueError("invalid thread id")
    elif res != 1:
        # """if it returns a number greater than one, you're in trouble,
        # and you should call it again with exc=NULL to revert the effect"""
        ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, None)
        raise SystemError("PyThreadState_SetAsyncExc failed")
    
    
def stop_thread(thread):
    _async_raise(thread.ident, SystemExit)

# # using select not to block in IO
# # so when a stop is called, abor cmd can be done immediately
# def if_abor(_sd,_tmp):
#     while True:
#         time.sleep(0.2)
#         if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
#             msg=sys.stdin.readline()
#             #print("read: ",msg)
#             time.sleep(0.2)
#             if msg[0:4]=="ABOR":
#                 _sd.send(msg.encode('utf-8'))
#                 return
#             else:
#                 #print("425 sorry, cmd cannot be sent. Transfering...")
        

 
        
def retr_file(_filepath,_sd_conn,_sd_ls,_sd):
    global isdownload
    #p_abor=threading.Thread(target=if_abor,args=(_sd_ls,0))
    #p_abor.start()
    global ifRest
    global restPos
    global file_mode
    
    isdownload=1
    with open(_filepath,'wb') as outfile:
        if ifRest!=0:
            outfile.seek(int(restPos))
            #print("resPos: ",restPos)
            #print("int(restPos): ",int(restPos))
            ifRest=0
            restPos=0
        while True:
            # if p_abor.is_alive()==False:
            #     break
            resv_data=_sd_conn.recv(bufferSize)
            if not resv_data:
                #stop_thread(p_abor)
                break
            outfile.write(resv_data)
    resv_message=_sd_ls.recv(bufferSize).decode()
    #print(resv_message)
    # if p_abor.is_alive()==True:
    #     stop_thread(p_abor)
    _sd_conn.close() 
    if file_mode==1:
        _sd.close()
        file_mode=-1

# return current directory
def ftp_pwd(_sd):
    _sd.send(("PWD"+'\r\n').encode('utf-8'))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    p=re.compile('.+?"(.+?)"')
    return p.findall(recv_msg)[0]

def ftp_cwd(_sd,_path):
    _sd.send(("CWD "+_path+'\r\n').encode('utf-8'))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)

def ftp_connect(_sd):
        try:
            # establish connection
            global host_ip
            global host_port
            _sd.connect((host_ip,host_port))
            recv_msg=_sd.recv(100).decode()
            print(recv_msg)
            if(recv_msg.startswith('2')):
                return 0
            else:
                return -1
        except:
            return -1
        
def ftp_login(_sd):
    _sd.send(("USER "+username+'\r\n').encode('utf-8'))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    _sd.send(("PASS "+email_address+'\r\n').encode('utf-8'))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    return 0

def ftp_quit(_sd):
    _sd.send(("QUIT"+'\r\n').encode('utf-8'))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)

def ftp_mkdir(_sd,_dir):
    _sd.send(("MKD "+_dir+'\r\n').encode("utf-8"))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    
def ftp_rmdir(_sd,_dir):
    _sd.send(("RMD "+_dir+'\r\n').encode("utf-8"))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    
def ftp_rename(_sd,_old,_new):
    _sd.send(("RNFR "+_old+"\r\n").encode("utf-8"))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)
    _sd.send(("RNTO "+_new+"\r\n").encode("utf-8"))
    recv_msg=_sd.recv(100).decode()
    print(recv_msg)


def ftp_upload(_sd,_file_name,passive=True):
    global sock_conn
    global conn_ip
    global conn_port
    global ifRest  
    global file_mode 
    global isupload
    global storsize
    global restPos
    isupload=1
    # if ifRest!=0:
    #     if restPos>=10000:
    #         storsize=restPos-10000
    #     else:
    #         storsize=0
    # else:
    #     storsize=0
    storsize=restPos
    if(passive):
        ftp_pasv(_sd)
        if ifRest:
            ftp_rest(_sd,restPos)
        _sd.send(("STOR "+os.path.split(_file_name)[1]+"\r\n").encode("utf-8"))
        # create the socket for data connection
        sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # bind a default port number: randomized by system
        # establish connection
        sock_conn.connect((conn_ip,conn_port))
        # receive msg about transfer start
        resv_message=_sd.recv(bufferSize).decode()
        if resv_message.startswith('150'):
            print(resv_message)
            with open(_file_name,'rb') as infile:
                if ifRest!=0:
                    infile.seek(int(restPos))
                    ifRest=0
                while True:
                    indata = infile.read(4096)
                    # print(indata)
                    sock_conn.send(indata)
                    storsize+=len(indata)
                    if not indata : break
            sock_conn.close()
            # get transfer complete msg
            resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        print("storrOver")
            
        
    else:
        ftp_port(_sd)
        if ifRest:
            ftp_rest(_sd,restPos)
        conn,addr=sock_conn.accept()
        # get msg about transfer start
        resv_message=_sd.recv(bufferSize).decode()    
        if resv_message.startswith('150'):      
            with open(_file_name,'rb') as infile:
                if ifRest!=0:
                    infile.seek(int(restPos))
                    ifRest=0
                while True:
                    indata = infile.read(4096)
                    # print(indata)
                    conn.send(indata)
                    stor_size+=len(indata)
                    if not indata : break
            conn.close()
            resv_message=_sd.recv(bufferSize).decode()          
            sock_conn.close()
        print(resv_message)
    
    file_mode=-1

def ftp_rest(_sd,_pos):
    _sd.send(("REST "+str(_pos)+'\r\n').encode("utf-8"))
    resv_message=_sd.recv(bufferSize).decode()
    print(resv_message)
    

def ftp_download(_sd,_file_name,passive=True):         
    global sock_conn
    global conn_ip
    global conn_port 
    global downloadPath
    global cur_download_size
    global ifRest
    global restPos
    if(passive):
        ftp_pasv(_sd)            
        if ifRest:
            ftp_rest(_sd,restPos)
        _sd.send(("RETR "+_file_name+'\r\n').encode("utf-8"))
        # create the socket for data connection
        sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # bind a default port number: randomized by system
        # establish connection
        sock_conn.connect((conn_ip,conn_port)) 
        resv_message=_sd.recv(bufferSize).decode()
        print(resv_message)    
        if resv_message.startswith('150'):
            cur_download_size=float(resv_message.split(" ")[-2][1:])
            
            retr_file(downloadPath+"/"+os.path.split(_file_name)[1],sock_conn,sock_ls,sock_conn)
    
    else:
        ftp_port(_sd)
        if ifRest:
                ftp_rest(_sd,restPos)
        _sd.send(("RETR "+_file_name+'\r\n').encode("utf-8"))
        conn,addr=sock_conn.accept()
        resv_message=_sd.recv(bufferSize).decode()
        print(resv_message)
        
        if resv_message.startswith('150'):
            cur_download_size=float(resv_message.split(" ")[-2][1:])

            retr_file(downloadPath+"/"+os.path.split(_file_name)[1],conn,sock_ls,sock_conn)
            
    ifRest=0


# get server's client ip and port from msg received (Used in PASV)
# return: ip and port
def parse_ip_and_port_PASV(resv_msg):
    ptn=re.compile(r'[(](.*?)[)]', re.S)
    ip_and_port=re.findall(ptn,resv_msg)[0].split(',')
    ip=ip_and_port[0]+'.'+ip_and_port[1]+'.'+ip_and_port[2]+'.'+ip_and_port[3]
    port=int(ip_and_port[4])*256+int(ip_and_port[5])
    return ip,port

def ftp_pasv(_sd):
    global conn_ip
    global conn_port
    _sd.send(("PASV"+'\r\n').encode("utf-8"))
    resv_message=_sd.recv(bufferSize).decode()
    print(resv_message)
    if resv_message.startswith('2'):
        conn_ip,conn_port=parse_ip_and_port_PASV(resv_message)       
        file_mode=0
    ifRest=0


def ftp_abor(_sd):
    #global p_download
    global isupload
    global storsize
    global restPos
    _sd.send("ABOR\r\n".encode("utf-8"))
    resv_message=_sd.recv(bufferSize).decode()
    print(resv_message)
    if isupload:
        restPos=int(resv_message.split()[-1][:-1])
    #stop_thread(p_download)

# get server's client ip and port from msg received
# return: ip and port
def parse_ip_and_port_PORT(resv_msg):
    ip_and_port=resv_msg.split()[1].split(',')
    ip=ip_and_port[0]+'.'+ip_and_port[1]+'.'+ip_and_port[2]+'.'+ip_and_port[3]
    port=int(ip_and_port[4])*256+int(ip_and_port[5])
    return ip,port

    
def ftp_port(_sd,_path):
    msg="PORT "+_path+'\r\n'
    sock_ls.send(msg.encode("utf-8"))
    resv_message=sock_ls.recv(bufferSize).decode()
    print(resv_message)
    if resv_message.startswith('2'):
        my_conn_ip,my_conn_port=parse_ip_and_port_PORT(msg)       
        # create the socket for data connection
        global sock_conn
        sock_conn.close()
        sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # bind a default port number: randomized by system
        sock_conn.bind((my_conn_ip,my_conn_port))
        sock_conn.listen()
        file_mode=1
    ifRest=0
 
def ftp_list(_sd,path='',passive=True):

    show_list=[]
    whole_str=""
    here_path='LIST '+str(path)
    
    global sock_conn
    sock_conn.close()
    
    if(passive):
        ftp_pasv(_sd)
        global conn_ip
        global conn_port
        _sd.send((here_path+'\r\n').encode('utf-8'))
        # create the socket for data connection
        sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        #print("socket over")
        # bind a default port number: randomized by system
        # establish connection
        sock_conn.connect((conn_ip,conn_port))
        #print("connect over")
        # receive msg about transfer start
        resv_message=_sd.recv(bufferSize).decode()
        print(resv_message)
        if resv_message.startswith('150'):
            while True:
                indata = sock_conn.recv(bufferSize)                
                if not indata:
                    break
                #print(indata.decode())
                whole_str+=indata.decode()
            sock_conn.close()
            # get transfer complete msg
            resv_message=_sd.recv(bufferSize).decode()
            print(resv_message)
                
    else:
        ftp_port(_sd)
        _sd.send(here_path.encode('utf-8'))
        conn,addr=sock_conn.accept()
        # get msg about transfer start
        resv_message=_sd.recv(bufferSize).decode()   
        print(resv_message) 
        if resv_message.startswith('150'):      
            while True:
                indata = sock_conn.recv(bufferSize)               
                if not indata : break
                #print(indata.decode())
                whole_str+=indata.decode()
            conn.close()
            resv_message=_sd.recv(bufferSize).decode()          
            sock_conn.close()
            print(resv_message)
    
    file_mode=-1 
    ifRest=0
    return whole_str.strip().split('\r\n')
    
    

class mainMenu(QWidget):
    
    def __init__(self):
        super().__init__()
        
        self.initUI()
        with open('./coffee.qss', 'r') as f:
            qssStyle=f.read()
        self.setStyleSheet(qssStyle)
        self.initData()
        
        self.step=0
        
        self.timer = QBasicTimer()
        self.setWindowTitle("File Service")
        
        self.ifUploadPathChanged=0
        
               
    def initUI(self):      
        self.layout=QVBoxLayout()
        self.head=QWidget()
        self.head_layout=QHBoxLayout()
        global username
        global host_ip
        global host_port
        if(username==''):
            self.head_layout.addWidget(QLabel("Welcome, anonymous! \r\n You are connecting to "+host_ip+" at "+str(host_port)+'.'))
        else:
            self.head_layout.addWidget(QLabel("Welcome, "+ username+"!"+"\r\n You are connecting to "+host_ip+" at "+str(host_port)+'.'))
        self.pBtn_disconnect=QPushButton()
        self.pBtn_disconnect.setText("disconnect")
        self.pBtn_disconnect.clicked.connect(self.disconnect)
        self.head_layout.addWidget(self.pBtn_disconnect)
        # self.pBtn_sendcmd=QPushButton()
        # self.pBtn_sendcmd.setText("send cmd")
        # self.pBtn_sendcmd.clicked.connect(self.sendcmd)
        # self.head_layout.addWidget(self.pBtn_sendcmd)
        self.head.setLayout(self.head_layout)
        self.layout.addWidget(self.head)
        # self.cmd_choose=QComboBox()
        # self.cmd_choose.addItems(["RETR","STOR","LIST","CWD","PWD","MKD","REST","RNFR","RNTO"])
        # self.layout.addWidget(self.cmd_choose)
        # self.layout.addWidget(QLineEdit())
        # self.layout.addWidget(QLabel("command list"))
        # self.cmd_list=QTextEdit()
        # self.layout.addWidget(self.cmd_list)
        
        # file list on host
        # using LIST command to show
        self.file_list_head_layout=QHBoxLayout()
        #self.file_list_head=QWidget()
        #self.file_list_head.setLayout(self.file_list_head_layout)
        self.file_list_head_layout.addWidget(QLabel("file_list"))
        self.file_list_nextdir=QPushButton()
        self.file_list_nextdir.setText("go")
        self.file_list_nextdir.clicked.connect(self.chooseAFile)
        self.file_list_head_layout.addWidget(self.file_list_nextdir)
        self.file_list_lastdir=QPushButton()
        self.file_list_lastdir.setText("back")
        self.file_list_lastdir.clicked.connect(self.goUpperDir)
        self.file_list_head_layout.addWidget(self.file_list_lastdir)
        self.file_list_mkdir=QPushButton()
        self.file_list_mkdir.setText("mkdir")
        self.file_list_mkdir.clicked.connect(self.goMakeDir)
        self.file_list_head_layout.addWidget(self.file_list_mkdir)
        self.file_list_rmdir=QPushButton()
        self.file_list_rmdir.setText("rmdir")
        self.file_list_rmdir.clicked.connect(self.goRemoveDir)
        self.file_list_head_layout.addWidget(self.file_list_rmdir)
        self.file_list_rename=QPushButton()
        self.file_list_rename.setText("rename")
        self.file_list_rename.clicked.connect(self.goRename)
        self.file_list_head_layout.addWidget(self.file_list_rename)
        self.layout.addLayout(self.file_list_head_layout)
        self.file_list=QListWidget()
        self.layout.addWidget(self.file_list)
        self.file_list.itemDoubleClicked.connect(self.chooseAFile)
        self.file_list.setDragEnabled(True) #设置拖拉
        self.file_list_tail_layout=QHBoxLayout()
        self.file_list_tail_layout.addWidget(QLabel("Download to: "))
        global downloadPath
        self.file_list_download_path=QLineEdit()
        self.file_list_download_path.setText(downloadPath)
        self.file_list_download_path.setReadOnly(True)
        self.file_list_tail_layout.addWidget(self.file_list_download_path)
        self.file_list_change_dir=QPushButton()
        self.file_list_change_dir.setText("change")
        self.file_list_change_dir.clicked.connect(self.changeDownloadPath)
        self.file_list_tail_layout.addWidget(self.file_list_change_dir)
        self.file_list_download=QPushButton()
        self.file_list_download.setText("download")
        self.file_list_download.clicked.connect(self.goDownload)
        self.file_list_tail_layout.addWidget(self.file_list_download)
        self.layout.addLayout(self.file_list_tail_layout)
        
        # self.layout.addWidget(QLabel("Upload to: "))
        # self.transfer_list=QTextEdit()
        # self.layout.addWidget(self.transfer_list)
        
        
        
        self.upload_file = QLineEdit()
        self.upload_file.setObjectName("Empty")
        self.upload_file.setText("")
        self.upload_file.setReadOnly(True) 
        self.pb_choosefile = QPushButton()
        self.pb_choosefile.setObjectName("browse")
        self.pb_choosefile.setText("browse") 
        self.layout.addWidget(QLabel("Choose a file to upload"))
        self.pb_choosefile.clicked.connect(self.openFileDlg)
        self.layout.addWidget(self.upload_file)
        self.layout.addWidget(self.pb_choosefile)
        
        # upload line
        self.upload_layout=QHBoxLayout()
        self.upload_layout.addWidget(QLabel("Upload to: "))
        self.upload_path=QLineEdit()
        self.upload_path.setReadOnly(True)
        self.upload_layout.addWidget(self.upload_path)
        self.pBtn_change_upload_dir=QPushButton()
        self.pBtn_change_upload_dir.setText("change")        
        self.pBtn_change_upload_dir.clicked.connect(self.changeUploadPath)
        self.upload_layout.addWidget(self.pBtn_change_upload_dir)
        self.pBtn_upload=QPushButton()
        self.pBtn_upload.setText("upload")
        self.pBtn_upload.clicked.connect(self.goUpload)
        self.upload_layout.addWidget(self.pBtn_upload)
        self.layout.addLayout(self.upload_layout)
        
        
        self.progress_layout=QHBoxLayout() 
        # Abort button
        self.pBtn_abor=QPushButton()
        self.pBtn_abor.setText("Abort")  
        self.pBtn_abor.clicked.connect(self.goAbort)    
        # Resume button
        self.pBtn_resume=QPushButton()
        self.pBtn_resume.setText("Resume") 
        self.pBtn_resume.clicked.connect(self.goResume)
        # Progress Bar
        self.pBar = QProgressBar(self)
        # self.pBar.setGeometry(30, 50, 200, 25)  
        self.layout.addWidget(self.pBar)
        self.progress_layout.addWidget(self.pBtn_abor)
        self.progress_layout.addWidget(self.pBtn_resume)
        self.layout.addLayout(self.progress_layout)
        self.pBar.hide()
        self.pBtn_abor.hide()
        self.pBtn_resume.hide()
        
        self.setLayout(self.layout)
        
        
        self.setGeometry(300, 300, 280, 170)
        # self.resize(800,600)
        self.setWindowTitle('Toggle button')
        self.center()
        self.show()
    
    def chooseAFile(self):
        global list_cur_path
        chooseFile=self.file_list.selectedItems()[0].text()
        if self.file_list.selectedItems():
            if chooseFile[0]=='-':
                #print("You tap on a file")
                pass
            else:
                if list_cur_path[-1]=='/':
                    list_cur_path+=self.file_list.selectedItems()[0].text().split(' ')[-1]
                else:
                    list_cur_path+='/'+self.file_list.selectedItems()[0].text().split(' ')[-1] 
                tmp_list=ftp_list(sock_ls,path=list_cur_path)
                self.file_list.clear()
                for each in tmp_list:
                    self.file_list.addItem(each)          
                #print("list_cur_path: ",list_cur_path)
        else:
            QMessageBox.about(self, "Hint", "Please choose a file!")
    
    def goUpperDir(self):
        global sock_ls
        global list_cur_path
        tag=0
        for i in range(len(list_cur_path)):
            if list_cur_path[i]=='/':
                tag=i
        if tag==0:
            list_cur_path='/'
        else:
            list_cur_path=list_cur_path[0:tag]
        self.listRefresh()
        
    
    def goMakeDir(self):
        global sock_ls
        global list_cur_path
        text, okPressed = QInputDialog.getText(self, "Make A New Directory","directory name:", QLineEdit.Normal, "")
        if okPressed and text != '':
            #print(text)
            if list_cur_path[-1]=='/':
                ftp_mkdir(sock_ls,list_cur_path+text)
            else:
                ftp_mkdir(sock_ls,list_cur_path+'/'+text)
            self.listRefresh()
            
    
    def goRemoveDir(self):
        global sock_ls
        global list_cur_path
        if self.file_list.selectedItems():
            tmp_old=self.file_list.selectedItems()[0].text().split(' ')[-1]
            if list_cur_path[-1]=='/':
                ftp_rmdir(sock_ls,list_cur_path+tmp_old)
            else:
                ftp_rmdir(sock_ls,list_cur_path+'/'+tmp_old)
            self.listRefresh()
        else:
            QMessageBox.about(self, "Hint", "Please choose a directory to remove!")
            
    
    # initialize file list and upload path: using LIST and pwd command
    def initData(self):
        global sock_ls
        global list_cur_path
        this_list=ftp_list(sock_ls,'')
        for each in this_list:
             self.file_list.addItem(each)      
        list_cur_path=ftp_pwd(sock_ls)
        self.upload_path.setText(list_cur_path)
    
    def center(self):  
        screen = QDesktopWidget().screenGeometry()
        size = self.geometry()
        # a little modification
        self.move((screen.width() - size.width()) / 2,
                  (screen.height() - size.height()) / 2)
    
    def disconnect(self):
        ftp_quit(sock_ls)
        self.close()
        #sys.exit(app.exec_())
        
        
    def sendcmd(self):
        pass
    
    def goRename(self):
        global sock_ls
        global list_cur_path
        tmp_old=self.file_list.selectedItems()[0].text().split(' ')[-1]
        text, okPressed = QInputDialog.getText(self, "Rename","New file name:", QLineEdit.Normal, "")
        if okPressed and text != '':
            #print(text)
            ftp_rename(sock_ls,list_cur_path+'/'+tmp_old,list_cur_path+'/'+text)
            self.listRefresh()
    
    
    def openFileDlg(self):
        # absolute_path is a QString object
        absolute_path = QFileDialog.getOpenFileName(self, 'Open file', 
            '.',"")
        #print(absolute_path[0])
        self.upload_file.setText(absolute_path[0])
    
    def changeDownloadPath(self):
        global downloadPath
        downloadPath=QFileDialog.getExistingDirectory(self,  "Download To",  downloadPath) # 起始路径
        self.file_list_download_path.setText(downloadPath)
        
    def changeUploadPath(self):
        if self.ifUploadPathChanged==0:
            self.upload_path.setReadOnly(False)
            self.pBtn_change_upload_dir.setText("save")
            self.ifUploadPathChanged=1
        else:
            self.pBtn_change_upload_dir.setText("change")
            self.ifUploadPathChanged=0
            ftp_cwd(sock_ls,self.upload_path.text())
            self.upload_path.setReadOnly(True)
        
    def timerEvent(self, *args, **kwargs):
        global cur_download_size
        global isdownload
        global isupload
        global storsize
        global storAllSize
        if self.step>=100:
            # 停止进度条
            isdownload=0
            isupload=0
            if isupload:
                time.sleep(1)
            # self.listRefresh()
            self.step=0
            self.timer.stop()
            self.pBar.hide()
            self.pBtn_abor.hide()
            self.pBtn_resume.hide()           
            return
        time.sleep(0.1)
        if isdownload:
            self.step=os.path.getsize(downloadPath+"/"+os.path.split(cur_download_file)[1])/cur_download_size*100
        elif isupload:
            self.step=storsize/storAllSize*100
        #print("timerEvent: ",self.step)
        # 把进度条每次充值的值赋给进图条
        self.pBar.setValue(self.step)
        
    def goUpload(self):
        global sock_ls 
        global downloadPath
        global cur_download_file
        global list_cur_path
        global storAllSize
        
        self.pBtn_abor.show()
        self.pBar.show()
        self.pBtn_resume.show()
        #print("self.step: ",self.step)
        
        storAllSize=os.path.getsize(self.upload_file.text())
        
        if self.ifUploadPathChanged==0:
            if self.upload_file.text():
                p_upload=threading.Thread(target=ftp_upload,args=(sock_ls,self.upload_file.text()))
                self.step=0
                p_upload.start()
                self.pBar.setValue(self.step)
                #print("self.step: ",self.step)
                time.sleep(0.1)
                self.timer.start(0,self)  
            else:
                QMessageBox.about(self, "Hint", "Please choose a file to upload!")
        else:
            QMessageBox.about(self, "Hint", "Please save your upload path!")
            
    def goAbort(self):
        global sock_ls
        ftp_abor(sock_ls)
        self.timer.stop()
        
    def goResume(self):
        global sock_ls
        global restPos
        global ifRest
        global cur_download_file
        global isdownload
        global isupload
        global storsize
        global storAllSize
        
        if isdownload:
            ifRest=1
            ##print("resumeStart!!!!!")
            restPos=os.path.getsize(downloadPath+"/"+os.path.split(cur_download_file)[1])
            p_download=threading.Thread(target=ftp_download,args=(sock_ls,cur_download_file)) 
            p_download.start()
            self.step=os.path.getsize(downloadPath+"/"+os.path.split(cur_download_file)[1])/cur_download_size*100
            self.timer.start(self.step,self)
            #print("resumeover!!!!!!!!!!!!!!!!!")
        elif isupload:
            ifRest=1
            #print("resumeStart!!!!!")
            #print("storsize resume: ",storsize)
            #print("restPos: ",restPos)
            #print("storsize: ",storsize)
            p_download=threading.Thread(target=ftp_upload,args=(sock_ls,self.upload_file.text())) 
            p_download.start()
            self.step=storsize/storAllSize*100
            self.timer.start(self.step,self)
            #print("resumeover!!!!!!!!!!!!!!!!!")
            
   
    def listRefresh(self):
        global sock_ls
        global list_cur_path
        this_list=ftp_list(sock_ls,path=list_cur_path)
        self.file_list.clear()
        for each in this_list:
            self.file_list.addItem(each)
        
    def goDownload(self):
        global sock_ls
        global downloadPath
        global cur_download_file
        global list_cur_path
        
        self.pBtn_abor.show()
        self.pBar.show()
        self.pBtn_resume.show()
        self.step=0
        self.pBar.setValue(self.step)
        #print("self.step: ",self.step)
        
        # p_timer=threading.Thread(target=self.timer.start,args=(0,self))
        # p_timer.start()
        if list_cur_path[-1]=='/':
            cur_download_file=list_cur_path+self.file_list.selectedItems()[0].text().split(' ')[-1]
        else:
            cur_download_file=list_cur_path+'/'+self.file_list.selectedItems()[0].text().split(' ')[-1]
        # ftp_download(sock_ls,cur_download_file)
        p_download=threading.Thread(target=ftp_download,args=(sock_ls,cur_download_file))  
        self.step=0
        p_download.start()
        self.pBar.setValue(self.step)
        #print("self.step: ",self.step)
        time.sleep(0.1)
        self.timer.start(0,self)        
        #print("downloadOVer!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
 
        
class startBox(QWidget):
    
    def __init__(self):
        super().__init__()
        
        self.initUI()
        
        
    def initUI(self):
        # wholePage=QHBoxLayout()
        # conn_wg=QWidget()
        layout=QVBoxLayout()
        form_wg=QWidget()
           
        form = QFormLayout()  
        self.ip_host_IP=QLineEdit()
        form.addRow("host IP:", self.ip_host_IP)
        self.ip_port=QLineEdit()
        form.addRow("port:",self.ip_port)
        self.ip_username=QLineEdit()
        form.addRow("username:",self.ip_username)
        self.ip_email_address=QLineEdit()
        form.addRow("email address: ",self.ip_email_address)
        
        self.hint=QLabel()
        # self.hint.setReadOnly(True)
        # self.hint.hide()
        
        self.pBtn_connect=QPushButton()
        self.pBtn_connect.setText("connect")
        self.pBtn_connect.clicked.connect(self.connect_and_login)
        
        form_wg.setLayout(form)
        layout.addWidget(form_wg)
        layout.addWidget(self.hint)
        layout.addWidget(self.pBtn_connect)
        
        self.setLayout(layout)
        
        self.center()
        self.resize(300,180)
        self.setWindowTitle('FTP START')
        self.show()
        
        
    def center(self):  
        screen = QDesktopWidget().screenGeometry()
        size = self.geometry()
        # a little modification
        self.move((screen.width() - size.width()/2) / 2,
                  (screen.height() - size.height()/2) / 2)
        
    def connect_and_login(self):
        global host_ip
        global host_port
        global username
        global email_address
        host_ip=self.ip_host_IP.text()
        host_port=int(self.ip_port.text())
        username=self.ip_username.text()
        email_address=self.ip_email_address.text()
        if(ftp_connect(sock_ls)==0):
            if(ftp_login(sock_ls)==0):
                self.hide()
                self.mainPage=mainMenu()
                self.mainPage.show()
            else:
                self.hint.setText("Failure to log in")
                self.hint.setAlignment(Qt.AlignCenter)
        else:
            self.hint.setText("Failure to reach server.")
            self.hint.setAlignment(Qt.AlignCenter)
        
        
        
        

if __name__ == '__main__':

    app = QApplication(sys.argv)
    
    with open('./coffee.qss', 'r') as f:
        qssStyle=f.read()
        
    startPage=startBox()
    startPage.show()
    startPage.setStyleSheet(qssStyle)

    sys.exit(app.exec_())
