#! /usr/bin/python3
# coding="utf-8"

import socket
import re
import os
import threading
import time
import ctypes
import inspect
import select
import sys

# global sock_ls
# global sock_conn
# sock_conn=socket.socket()
bufferSize = 4096
# set where you would like to put your downloaded file
file_dir='/home/wangyingqi/桌面/'
# -1 means no connection set; 0 means PASV; 1 means PASS
file_mode=-1
conn_ip=''
conn_port=0
my_conn_ip=''
my_conn_port=0
ifRest=0
restPos=0
pthread=[]



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


def if_abor(_sd,_tmp):
    while True:
        time.sleep(0.2)
        if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
            msg=sys.stdin.readline()
            print("read: ",msg)
            time.sleep(0.2)
            if msg[0:4]=="ABOR":
                _sd.send(msg.encode('utf-8'))
                return
            else:
                print("425 sorry, cmd cannot be sent. Transfering...")
        
        
def retr_file(_filepath,_sd_conn,_sd_ls,_sd):
    p_abor=threading.Thread(target=if_abor,args=(_sd_ls,0))
    p_abor.start()
    global ifRest
    global restPos
    global file_mode
    with open(_filepath,'wb') as outfile:
        if ifRest!=0:
            outfile.seek(restPos)
            ifRest=0
        while True:
            if p_abor.is_alive()==False:
                break
            resv_data=_sd_conn.recv(bufferSize)
            if not resv_data:
                stop_thread(p_abor)
                break
            outfile.write(resv_data)
    resv_message=_sd_ls.recv(bufferSize).decode()
    print(resv_message)
    if p_abor.is_alive()==True:
        stop_thread(p_abor)
    _sd_conn.close() 
    if file_mode==1:
        _sd.close()
        file_mode=-1
    
  
    
# get server's client ip and port from msg received (Used in PASV)
# return: ip and port
def parse_ip_and_port_PASV(resv_msg):
    ptn=re.compile(r'[(](.*?)[)]', re.S)
    ip_and_port=re.findall(ptn,resv_msg)[0].split(',')
    # print(ip_and_port)
    ip=ip_and_port[0]+'.'+ip_and_port[1]+'.'+ip_and_port[2]+'.'+ip_and_port[3]
    port=int(ip_and_port[4])*256+int(ip_and_port[5])
    return ip,port

# get server's client ip and port from msg received
# return: ip and port
def parse_ip_and_port_PORT(resv_msg):
    ip_and_port=resv_msg.split()[1].split(',')
    ip=ip_and_port[0]+'.'+ip_and_port[1]+'.'+ip_and_port[2]+'.'+ip_and_port[3]
    port=int(ip_and_port[4])*256+int(ip_and_port[5])
    return ip,port

# create socket for data connection and connect to it (Used in PASV mode)
# return: boolean (if no error)
def create_and_connect_conn_socket(ip,port):
    try:
        # create the socket for data connection
        sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # establish connection
        sock_conn.connect((ip,port))
    except:
        print("cannot reach the server")
        return False
    return True

def copy_file(resv_data,msg):
    file_name=os.path.split(msg[5:])[1]
    print(type(resv_data))
    #dt=resv_data.decode().split('\n')
    # dt2=dt.join('\n')
    with open(file_dir+file_name,'wb') as outfile:
        outfile.write(resv_data)


try:
    # create the socket for control
    sock_ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # establish connection
    sock_ls.connect(('166.111.80.237',41121))
except:
  print("cannot reach the server")
  
# get welcome msg
print(sock_ls.recv(bufferSize).decode().strip())

while(1):            
    msg=input(">>").strip()
    #sock_ls.send(msg.encode("utf-8"))

    if(msg[0:4]=="PASV"):    
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        if resv_message.startswith('2'):
            conn_ip,conn_port=parse_ip_and_port_PASV(resv_message)       
            print(conn_ip , conn_port)
            file_mode=0
        ifRest=0
        
    elif(msg[0:4]=="PORT"):
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        if resv_message.startswith('2'):
            my_conn_ip,my_conn_port=parse_ip_and_port_PORT(msg)       
            print(my_conn_ip , my_conn_port)
            # create the socket for data connection
            sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # bind a default port number: randomized by system
            sock_conn.bind((my_conn_ip,my_conn_port))
            sock_conn.listen()
            file_mode=1
        ifRest=0
        
    elif(msg[0:4]=="RETR"):
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        # receive msg about transfer start
            
        if(file_mode==0):
            # create the socket for data connection
            sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # bind a default port number: randomized by system
            # establish connection
            sock_conn.connect((conn_ip,conn_port)) 
            resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            
            if resv_message.startswith('150'):
                file_name=os.path.split(msg[5:])[1]
                # write data to file
                #p_retr = threading.Thread(target=retr_file, args=(file_dir+file_name,sock_conn,sock_ls,sock_conn))
                retr_file(file_dir+file_name,sock_conn,sock_ls,sock_conn)
                #pthread.append(p_retr)
                #p_retr.start()
        
        elif(file_mode==1):
            conn,addr=sock_conn.accept()
            # get times
            resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            if resv_message.startswith('150'):
                file_name=os.path.split(msg[5:])[1]
                # p_retr = threading.Thread(target=retr_file, args=(file_dir+file_name,conn,sock_ls,sock_conn))
                # pthread.append(p_retr)
                # p_retr.start()
                retr_file(file_dir+file_name,conn,sock_ls,sock_conn)
            #     with open(file_dir+file_name,'wb') as outfile:
            #         if ifRest!=0:
            #             outfile.seek(restPos)
            #             ifRest=0
            #         while True:
            #             resv_data=conn.recv(bufferSize)
            #             if not resv_data:
            #                 break
            #             outfile.write(resv_data)           
            #     resv_message=sock_ls.recv(bufferSize).decode()
            #     conn.close()
            #     print(resv_message)
            # sock_conn.close()
            
        else:
            resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            file_mode=-1
        
    elif(msg[0:4]=='STOR'):
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
            
        if(file_mode==0):
            
            # create the socket for data connection
            sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # bind a default port number: randomized by system
            # establish connection
            sock_conn.connect((conn_ip,conn_port))
            # receive msg about transfer start
            resv_message=sock_ls.recv(bufferSize).decode()
            if resv_message.startswith('150'):
                print(resv_message)
                file_path=msg[5:].strip()
                with open(file_path,'rb') as infile:
                    if ifRest!=0:
                        infile.seek(restPos)
                        ifRest=0
                    while True:
                        indata = infile.read(4096)
                        # print(indata)
                        sock_conn.send(indata)
                        if not indata : break
                sock_conn.close()
                # get transfer complete msg
                resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            
        
        elif(file_mode==1):
            conn,addr=sock_conn.accept()
            # get msg about transfer start
            resv_message=sock_ls.recv(bufferSize).decode()    
            if resv_message.startswith('150'):      
                file_path=msg[5:].strip()
                with open(file_path,'rb') as infile:
                    if ifRest!=0:
                        infile.seek(restPos)
                        ifRest=0
                    while True:
                        indata = infile.read(4096)
                        # print(indata)
                        conn.send(indata)
                        if not indata : break
                conn.close()
                resv_message=sock_ls.recv(bufferSize).decode()          
                sock_conn.close()
            print(resv_message)
            
        else:
            resv_message=sock_ls.recv(bufferSize).decode() 
            print(resv_message)
        file_mode=-1
        
    elif(msg=="QUIT"):
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        break
    
    elif(msg[0:4]=="LIST"):
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        
        if(file_mode==0):
             # create the socket for data connection
            sock_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # bind a default port number: randomized by system
            # establish connection
            sock_conn.connect((conn_ip,conn_port))
            # receive msg about transfer start
            resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            if resv_message.startswith('150'):
                while True:
                    indata = sock_conn.recv(bufferSize)                
                    if not indata:
                        break
                    print(indata.decode())
                sock_conn.close()
                # get transfer complete msg
                resv_message=sock_ls.recv(bufferSize).decode()
                print(resv_message)
                   
        elif(file_mode==1):
            conn,addr=sock_conn.accept()
            # get msg about transfer start
            resv_message=sock_ls.recv(bufferSize).decode()   
            print(resv_message) 
            if resv_message.startswith('150'):      
                while True:
                    indata = sock_conn.recv(bufferSize)               
                    if not indata : break
                    print(indata.decode())
                conn.close()
                resv_message=sock_ls.recv(bufferSize).decode()          
                sock_conn.close()
                print(resv_message)
                    
        else:
            resv_message=sock_ls.recv(bufferSize).decode()
            print(resv_message)
            
        file_mode=-1 
        ifRest=0
    elif msg[0:4]=='REST':
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        if resv_message.startswith("3"):
            ifRest=1
            restPos=int(msg.split()[1])
            print("restPos: ",restPos)
    # elif msg[0:4]=='ABOR':
    #     p_abor=threading.Thread(target=sock_ls.send, args=(msg.encode('utf-8')))
    #     p_retr.start()
    #     # ifRest=0
    #     pass
    else:
        sock_ls.send((msg.strip()+'\r\n').encode("utf-8"))
        resv_message=sock_ls.recv(bufferSize).decode()
        print(resv_message)
        ifRest=0
    
sock_ls.close()

