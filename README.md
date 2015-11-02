# ChatSimple
A simple chat client-server chat tool for homework

## How to build
- Make sure you have install libevent2. On Ubuntu, you can install it using: sudo apt-get install libevent-dev
- Change to the root directory of the project, run: make all


## Start the program
- server Command options: ./chatSimple [port] [-d] [debug]
    1. port: the listened port
    2. -d: run it at background
    3. debug: output some information for debug
- log: If you run it as backgroup, the log will output to current direction, file chatSimple.log  
- client command options: ./client [server-ip] [server-port] [login-name] [debug]
    1. server-ip
    2. server-port
    3. login-name
    4. debug
    
## Protocol
- basic message structure is TLV: 
-TLV(type-length-value) structure
    - type: int, 4 bytes
    - len : int, 4 bytes
    - value: any data, 'len' bytes
    
- user login:
    - type: 0
    - len: less than 32 bytes
    - value: one all more tlv, usaully a string tlv for user name.
- text message:
    - type: 1
    - len: less than pow(2,16)
    - type: one or more tvl, usaully a string tlv for target user name and a string tlv for content
- file message:
    - type: 2
    - len: less than pow(2,31)
    - value: one or more tvl
- server response:
    - type: 3
    - len: not tow long
    - value: string

## Example
```
server:
./chatSimple 3333

client joe:
jason@vb:~/code/ChatSimple$ ./client 127.0.0.1 3333 joe
>hello! 
>joe
*To user: 
>[tj]:
>hi

>[tj]:
>how are you?
tj
*content, a line: hi
*To user: tj
*content, a line: I am find. thanks and you?
*To user: 



client tj:
jason@vb:~/code/ChatSimple$ ./client 127.0.0.1 3333 tj
>hello! 
>tj
*To user: joe
*content, a line: hi
*To user: joe
*content, a line: how are you?
*To user: 
>[joe]:
>hi

>[joe]:
>I am find. thanks and you?

```
    
    