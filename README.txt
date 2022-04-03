To run shellfyre first comple shellfyre.c by writing 
gcc shellfyre.c -o shellfyre
./shellfyre

Part 1

To run simple commands such as mkdir, sleep, pwd etc. You can just write it like it is the normal shell.
Writing & at the end of the command makes it run in the background. 


Part 2 - filesearch
To run filesearch:

filesearch "foo"   --->  prints the path of files with matching string in the current directory
filesearch -r "foo" --->  prints the path of files with matching string in the current and sub directory
filesearch -o "foo" --->  prints the path of files with matching string in the current directory and opens them
filesearch -r -o "foo" --->  prints the path of files with matching string in the current and sub directory and opens them
filesearch -o -r "foo" --->  prints the path of files with matching string in the current and sub directory and opens them


Part 2 - cdh
To run cdh:

cdh
Then choose the number or the letter in front of the directory you want to go to

Note: Running this code will create a txt file keeping track of directories you visited.

Part 2 - take
To run take:

take A/B/C
if ./A/B/C exists changes directory to that
if not creates that directory and changes it to that

Part 2 - joker
To run joker:

joker ---> Pops a joke notification every 15 minute

Part 2 - shoplist
To run shoplist:

shoplist -a ITEM_YOU_WANT NUMBER_OF_ITEM (for example: shoplist -a egg 5) -----> adds the item to the shopping list
shoplist -l ----> prints your shopping list
shoplist -d ----> deltes every item in the shopping list

Part 2 - carry
To run carry:

carry ./aa/Eray.txt ./A/B/C ----> carries ./aa/Eray.txt into ./A/B/C then deletes the first file in the inital directory

Part 3
To run pstraverse:

1- make
2- pstraverse your_root_pid your_traversel_mode (for example: pstraverse 1 -d)

Note: We could not manage to do it using ioctl therefore this command can be run only once per shellfyre session. To run again you need to exit and get back in the shell.







