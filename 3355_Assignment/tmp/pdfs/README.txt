
=== PAGE 1 ===
Software Security Assignment
Introduction
This assignment includes:
1. Task 1 – SecureAI MathOracle 40 points
2. Task 2 – MindVault, the Memory Clinic 30 points
3. Task 3 – Secure Casino 30 points
Total:100 points.
Read each task carefully and exploit the program’s vulnerabilities to complete it. We
provide source code to help you understand the program, but it may differ slightly from
the code used to build the executable. Use the supplied binaries for the attacks; do not re-
build them from the source code. Run them on the course serverworkbench3.cs.hku.hk
or in your own x86-64 Linux environment. SeeTutorial VM.pdffor setup instructions.
Each task ships aflag.txtfile next to the binary, because the program itself reads
it when you succeed. Simply opening the flag file is not a solution: your report must
demonstrate that you obtained the flag by exploiting the vulnerabilities of the program.
Submission
Please submit a report in PDF format to answer the questions listed in each task. Include
screenshots wherever a question asks for them, and put your final exploit scripts (if any)
in an appendix of the same PDF. The report must be your own work.
1
=== PAGE 2 ===
Environment and Tooling
Usegdbfor debugging andpwntoolsfor Python scripts that interact with the programs.
Both are pre-installed on the course server. For setup instructions, seeTutorial VM.pdf,
section A.4 (server) or B.1 (your own Linux machine).
All tasks can be debugged with plain GDB. We recommendpwndbg, a GDB plugin
that makes registers, stack frames and heap chunks easier to inspect. Commands such
asstack,telescopeandvis-heap-chunksrequire this plugin. With thegdbalias
fromTutorial VM.pdf, section A.4, thegdbcommands below launch pwndbg directly.
Without the alias, use~/pwndbg/bin/pwndbgin place ofgdbfor the portable version.
Run each binary from its own task directory so it can findflag.txt. Run your
exploit script from that directory too.
Usepwntoolsto send and receive bytes, including non-printable characters:
from pwn import *
p = process("./mathoracle") # start the program
p.recvuntil(b"Your name: ") # wait for the prompt
p.sendline(b"Alice") # send bytes followed by a newline
Usesend()for exact bytes andsendline()to add a newline. On x86-64, integers
are stored in little-endian order:
payload = b"A" * 32 + p32(0) + p64(0x555555555000)
# p32(x): pack the integer x into 4 little-endian bytes
# p64(x): pack the integer x into 8 little-endian bytes (x86-64 addresses)
leaked = u64(p.recvn(8)) # decode exactly 8 raw bytes as an integer
This decoding example assumes the next eight bytes are raw data. For a text address
printed by%p, useint(text, 16)instead.
GDB disables ASLR by default. Use it to inspect memory and measure offsets, but
do not copy its concrete addresses or canary values into your exploit. Leak the values
through the program and use them in thesame run; values from a separate GDB session
or an earlier run will not work.
2
=== PAGE 3 ===
Calling Conventions – x86-32 vs. x86-64
The lectures introduce 32-bit x86 cdecl, which passes arguments on the stack. This
assignment uses Linux x86-64 (System V AMD64), which passes the first six integer or
pointer arguments in registers. The basic ideas of stack frames and buffer overflows still
apply, but addresses are eight bytes wide and the layouts differ.
This section compares the two conventions using integer arguments and introduces
the debugger commands used in the assignment.
The example program
#include <stdio.h>
long foo(long a, long b, long c, long d, long e, long f, long g)
{
return a + b + c + d + e + f + g;
}
int main(void)
{
long r = foo(1, 2, 3, 4, 5, 6, 7);
printf("foo returned %ld\n", r);
return 0;
}
Save this example ascallconv.c. Its seven integer arguments show both register
and stack argument passing on x86-64.
gcc -g -O0 -fno-omit-frame-pointer -o callconv64 callconv.c
gcc -g -O0 -fno-omit-frame-pointer -m32 -o callconv32 callconv.c
The course server supports 32-bit compilation. Compile both versions there and follow
the debugger steps yourself; the practice will help with the assignment.
Where the arguments live
On x86-32 cdecl,mainplaces all seven arguments on the stack in reverse order: 7 first,
1 last. Thecallinstruction then pushes the return address. Atfoo’s first instruction,
[esp]holds the return address,[esp+4]holds argument 1, and the remaining arguments
follow at increasing addresses, four bytes apart.
On x86-64,mainputs arguments 1–6 inRDI,RSI,RDX,RCX,R8andR9, in that order.
Atfoo’s first instruction,[rsp]holds the return address and[rsp+8]holds argument 7.
3
=== PAGE 4 ===
The first six arguments arrive in registers; the function may later copy them to the stack.
In these examples, the return value is inEAX(32-bit) orRAX(64-bit). After the
function establishes its frame withpush ebp/rbpandmov ebp, espormov rbp, rsp,
arguments can also be located relative to the frame pointer, as shown in the table. Before
that, the frame pointer still belongs to the caller.
x86-32 (cdecl) x86-64 (System V AMD64)
Arguments 1–6 on the stack, pushed right-
to-left
inRDI,RSI,RDX,RCX,R8,R9
Arguments 7+ on the stack on the stack, above the return ad-
dress
After prologue[ebp+8],[ebp+0xc], . . . registers, or[rbp+0x10], . . . for 7+
Return valueEAX RAX
Cleanup caller restores stack caller restores stack (7+)
Callee-savedEBX,ESI,EDI,EBP RBX,RBP,R12–R15
Word size 4 bytes 8 bytes
Alignment depends on the ABIRSP16-byte aligned beforecall
The session on x86-64
At the debugger 1 — start and stop.
gdb -q ./callconv64 # start gdb
break *foo # the star: stop at foo’s FIRST machine instruction
run # run until the breakpoint hits
Figure 1: x86-64: the session starts and the breakpoint hits atcallconv.c:4.
4
=== PAGE 5 ===
In Figure 1, GDB displaysa=0, ..., f=0, g=7. In this build, the debug information
refers to stack slots for the first six parameters, but those slots have not been initialized
yet. Their actual incoming values are in the registers. GDB’s parameter display at this
early stop may differ between builds.
At the debugger 2 — the context panels.
Every time the program stops,pwndbgprints three panels by itself, without being
asked: REGISTERS, DISASM and STACK. The next two figures are the first two panels
of this very stop; read the prose under each before moving on.
Figure 2: x86-64: the REGISTERS panel at the stop.
The REGISTERS panel shows the incoming arguments directly:RDI=1,RSI=2,
RDX=3,RCX=4,R8=5 andR9=6. Use these registers to check arguments 1–6 at entry.
Figure 3: x86-64: the DISASM panel at the same stop.
The DISASM panel shows the function prologue.push rbp; mov rbp, rspestab-
lishes the frame. Later, instructions such asmov [rbp-8], rdicopy the register argu-
ments into stack slots; they do not remove the values from the registers.break *foo
stops before this prologue. Usesito step one instruction at a time and observe the
changes.
5
=== PAGE 6 ===
At the debugger 3 — dump the stack by hand.
stack 12 # show 12 pointer-sized words from the stack pointer
Learn this command well: it is the one you will use in Task 1 to measure where the
canary and the return address sit.
Figure 4: x86-64:stack 12at the same stop.
In Figure 4, word 0 at[rsp]is the return address (main+51, wheremainresumes
after the call). Word 1 is argument 7. The later rows show higher addresses in the
caller’s stack. Arguments 1–6 arrive in registers in this example.
The same session on x86-32
At the debugger 4 — repeat on x86-32.
gdb -q ./callconv32 # the 32-bit build of the same program
break *foo
run
Figure 5: x86-32: same program, same breakpoint.
6
=== PAGE 7 ===
Figure 5 displaysa=1, ..., g=7. All seven arguments have already been placed on
the stack, so GDB can read them at this stop. Compare their stack locations with the
register arguments in the x86-64 example.
Figure 6: x86-32: the REGISTERS panel at function entry.
In this cdecl example, the arguments are passed on the stack. Do not use the values
inEAX,ECXorEDXto identify incoming arguments; those registers are not used for
argument passing here.
Figure 7: x86-32: the DISASM panel fetches the arguments from the stack.
The DISASM panel loads arguments from[ebp+8],[ebp+0xc]and later stack slots,
four bytes apart.
At the debugger 5 — the same stack dump.
stack 12
Figure 8: x86-32:stack 12at the same stop.
7
=== PAGE 8 ===
Compare Figures 4 and 8: the x86-64 stack contains argument 7 in eight-byte slots,
while the x86-32 stack contains all seven arguments in four-byte slots. In both dumps,
the return address is at the stack pointer because we stopped before the prologue.
Other useful pwndbg commands
•regs: show register values.
•telescope <addr> N: showNpointer-sized words and follow pointers.
•vmmap: show mapped memory regions and their permissions.
•checksec: show protections such as canaries, NX and PIE.
•heap: list heap chunks, their sizes and allocation states.
•tcachebins: show freed chunks held in the thread’s tcache.
•vis-heap-chunks: show heap contents with chunk boundaries and colours.
For details and examples, see the pwndbg command reference. Task 2 includes a short
heap-debugging example.
8
=== PAGE 9 ===
Task 1 – SecureAI MathOracle (40 Points)
MathOracle is SecureAI’s “automated math oracle”. When you start it, it prints a
terminal banner, grants you 10 experience credits on your first visit, and asks for your
name. Internally your session is a small structure:
struct account {
char name[16];
int balance;
};
The program then reads your name with a generous limit:
printf("Your name: ");
read(0, user.name, 0x20);
If your balance is zero, the program asks for feedback and prints it back to you. It
then still accepts a math question, but refuses to answer without sufficient credit.
Your goal:redirect execution to a function that is never called normally and make
it printflag.txt. You will need to preserve the stack canary and calculate the target
address from the randomized executable base.
Guidance
•Review the buffer-overflow and format-string lectures. Use%N$pto select argument
positionN, counting from 1 after the format string. This is an argument index, not
a byte offset into the stack.
•Probe positions with strings such as%40$p %41$p %42$p. Compare the output
with the canary and saved return address inspected in GDBin the same run.
•Afterfeedbackestablishes its frame,[rbp]containschat’s saved frame pointer.
Use that value to inspectchat’s frame. Check the disassembly to locate its canary
and saved return address.
•Measure the buffer-to-canary and buffer-to-return-address distances in GDB. Pre-
serve the canary in your second overflow so the function can return. Disassemble
mainto identify the leaked return address: it points to the instruction after a
call. With PIE, computebase = leaked address - code offset, thentarget
= base + target offset. Use offsets relative to the executable base, not byte
positions in the file.
•read()may return fewer bytes than requested. Sending all stages at once may let
an earlier read consume later input. Send each stage after its prompt.
9
=== PAGE 10 ===
Report (40 Points)
1. Identify the buffer-overflow and format-string vulnerabilities. Give the source line
numbers for both overflow sites and the unsafe format-string call, and explain how
each contributes to the attack. (8 points)
2. Show your gdb probing process that identifies the vararg positions of the stack
canary and of the leaked code pointer. Include screenshots. (8 points)
3. Which instruction inmaindoes the leaked code pointer point to? Explain how you
turn it into the load base and then into your target address. (8 points)
4. Draw the stack frame of the vulnerable function at the moment of the second
overflow (buffer, canary, saved frame pointer, return address) and justify every
byte of your payload. (10 points)
5. Provide a screenshot of a successful run in which the binary prints the flag. (6
points)
10
=== PAGE 11 ===
Task 2 – MindVault, the Memory Clinic (30 Points)
MindVault is a long-term memory clinic: you can store memories, read them back, and
delete them. Every memory has exactly the same size and is stored as:
struct note {
long len; // first 8 bytes of the allocated user area
char data[]; // the memory itself
};
Reading a memory uses its length field, capped at 512 bytes:
long len = n->len;
if (len <= 0) { puts("(empty note)"); return; }
if (len > MAXDUMP) len = MAXDUMP;
write(1, n->data, len);
Deleting a memory releases it:
free(mem[idx]);
puts("Memory deleted. It’s gone forever. Trust us.");
The clinic also offers a “private inference” option. It produces a classified result (a
line starting withTOP SECRET:) that is stored somewhere on the heap, and no menu
option will ever hand you that pointer.
Your goal:make the Read menu option print the classified line, using only the menu
operations.
Guidance
•Review the use-after-free lifecycle slide: allocation, deallocation (dangling pointer),
use. Find the line that creates the dangling pointer.
•In the course server’s glibc allocator, these freed notes enter a small-block cache
calledtcache. The first 16 bytes of their user area hold a linked-list entry (offset
0) and a double-free detection key (offset 8). The link is encoded, so it may not
look like a normal pointer. Inspect it withx/4gx mem[idx]in GDB and compare
it with thelenfield thatshow()uses.
•Allocations may use fresh space or reuse freed chunks. Inspect where your two
notes and the classified scratch pad are placed, and how your operation order
affects whether the read can reach the classified data.
•The 512-byte cap limits how much a single read may print, but ask yourself what
it doesnotlimit.
11
=== PAGE 12 ===
•In a separate run, delete the same index twice and read the abort message. It
names the allocator mechanism that detects the double free.
•Note that the sizes used for the notes and for the classified scratch pad are different;
check what that means for where the scratch pad can come from.
Heap tools: a short example
•heap: list heap chunks, their sizes and allocation states.
•vis-heap-chunks: show chunk boundaries and memory contents in colour.
•tcachebins: show freed chunks cached for reuse.
•malloc-chunk <addr>: inspect the chunk whose header is ataddr.
•x/4gx <addr>: read four eight-byte values (a standard GDB command).
Figure 9: Example output: raw memory, chunk details, tcache and heap visualisation.
Heap visualisation command reference.
12
=== PAGE 13 ===
Report (30 Points)
1. Which source line creates the dangling pointer? What exactly doesmem[idx]
contain after a delete? (5 points)
2. When a note is freed into tcache, what does the allocator write into its first 16
bytes of user data? What is each field used for? (5 points)
3. Explain why reading a deleted memory can print data far beyond the chunk itself.
What precisely is wrong with the 512-byte cap? (5 points)
4. Draw the relevant heap layout after each operation in your attack: show your
two notes, the classified chunk when allocated, and the top chunk (remaining free
space at the heap’s end). Explain how your operation order lets the read reach the
classified data. (5 points)
5. What happens when you delete the same index twice, and how does the allocator
detect it? (5 points)
6. Provide screenshots of a successful run in which the classified line (and hence the
flag) is leaked. (5 points)
13
=== PAGE 14 ===
Task 3 – Secure Casino (30 Points)
The house always wins – unless you refuse to play fair. Secure Casino starts you with
$100. The vault at the back of the house opens only for players who (a) hold at least
$1,000,000 in cash and (b) have vault access granted by their loaded profile.
The shop calculates bulk prices using 32-bit signed integers, even though the program
itself is a 64-bit executable:
int total = item_price[id] * qty; // qty is limited to 1..65535
if (total > g_balance) { puts("Not enough cash."); return; }
g_balance -= total;
Your profile, including vault authorization and your name, lives inprofile.datand
is protected by a signatureSIGthat the program verifies on every reload. The reload
routine works in two phases:
/* check */ fopen + read + verify SIG ... fclose
puts("Signature OK. Contacting notary server...");
usleep(300000); // the notary server is very slow
/* use */ fopen + read again -> apply K= / B= ... fclose
The vault key is not for sale; vault authorization is loaded from the profile file.
Your goal:open the vault and make the program print the flag.
Guidance
•Signed integer overflow is undefined behaviour in C. For this task, analyze the
32-bit multiplication and wrap-around observed in the supplied binary.
•The receipt prints the wrapped total – watch its sign. A price that the check
considers “affordable” is not necessarily positive.
•The reload routine opens the profile file twice, with a slow step in between. The
program’s own output tells you when the window between check and use is open;
use that as the timing signal for swapping the file from a second terminal or thread.
Report (30 Points)
1. Explain the integer overflow for the quantity you chose: show the wrap-around at
bit/hex level, explain why the balance check lets it pass, and include the register’s
printed receipt. (8 points)
2. Give at least two other quantities that each raise the balance from$100 to at least
$1,000,000 through the same vulnerability. State the item and explain how you
14
=== PAGE 15 ===
found the quantities. (4 points)
3. Identify the check and use operations in the reload routine. Explain the approxi-
mately 300 ms delay between them and which output you used to trigger the file
change. Describe your attack and include screenshots. (10 points)
4. Propose source-code patches in diff format that eliminate both vulnerabilities. Sub-
mit the diff in your report; no rebuilt binary is required. (5 points)
5. Provide a screenshot of the vault opening and printing the flag. (3 points)
15