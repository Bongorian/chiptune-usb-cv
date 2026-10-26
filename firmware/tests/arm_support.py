"""Correct functional stubs for RP2040 ROM arithmetic/memory and SIO division.
These do not model ROM latency and must never be used to certify device WCET.
"""
import math
import struct
from collections import Counter
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_LR,UC_ARM_REG_PC

def runtime_hooks(image,uc,counts=None):
 counts=counts if counts is not None else Counter()
 def ret(machine,value=0,r1=None):
  machine.reg_write(UC_ARM_REG_R0,value&0xffffffff)
  if r1 is not None:machine.reg_write(UC_ARM_REG_R1,r1&0xffffffff)
  machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
 def hook(name,fn):
  if name in image.symbols:
   def wrapped(machine,address,size,data):counts[name]+=1;fn(machine)
   image.hook(uc,name,wrapped)
 def f(machine,reg):return struct.unpack('<f',struct.pack('<I',machine.reg_read(reg)))[0]
 def bits(value):
  try:return struct.unpack('<I',struct.pack('<f',value))[0]
  except OverflowError:return 0x7f800000 if value>0 else 0xff800000
 def signed(n):return n if n<0x80000000 else n-0x100000000
 def memory(copy):
  def run(m):
   dest,src,n=(m.reg_read(r)for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2))
   if n:m.mem_write(dest,bytes(m.mem_read(src,n))if copy else bytes([src&255])*n)
   ret(m,dest)
  return run
 hook('__wrap_memcpy',memory(True));hook('__wrap_memset',memory(False));hook('__wrap_memmove',memory(True))
 def divide(is_signed):
  def run(m):
   a,b=(m.reg_read(r)for r in (UC_ARM_REG_R0,UC_ARM_REG_R1))
   if is_signed:a,b=signed(a),signed(b)
   assert b,'ARM divide by zero'
   q=abs(a)//abs(b)
   if(a<0)!=(b<0):q=-q
   ret(m,q,a-q*b)
  return run
 hook('__wrap___aeabi_uidivmod',divide(False));hook('__wrap___aeabi_idivmod',divide(True))
 for suffix,op in [('eq',lambda a,b:a==b),('lt',lambda a,b:a<b),('le',lambda a,b:a<=b),('gt',lambda a,b:a>b),('ge',lambda a,b:a>=b),('un',lambda a,b:math.isnan(a)or math.isnan(b))]:
  hook('__wrap___aeabi_fcmp'+suffix,lambda m,op=op:ret(m,int(op(f(m,UC_ARM_REG_R0),f(m,UC_ARM_REG_R1)))))
 for suffix,op in [('add',lambda a,b:a+b),('sub',lambda a,b:a-b),('mul',lambda a,b:a*b),('div',lambda a,b:a/b if b else math.copysign(math.inf,a))]:
  hook('__wrap___aeabi_f'+suffix,lambda m,op=op:ret(m,bits(op(f(m,UC_ARM_REG_R0),f(m,UC_ARM_REG_R1)))))
 hook('__wrap___aeabi_i2f',lambda m:ret(m,bits(float(signed(m.reg_read(UC_ARM_REG_R0))))))
 hook('__wrap___aeabi_ui2f',lambda m:ret(m,bits(float(m.reg_read(UC_ARM_REG_R0)))))
 hook('__wrap___aeabi_f2iz',lambda m:ret(m,int(f(m,UC_ARM_REG_R0))))
 hook('__wrap___aeabi_f2uiz',lambda m:ret(m,int(f(m,UC_ARM_REG_R0))))
 def d(m,first):
  regs=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
  return struct.unpack('<d',struct.pack('<II',m.reg_read(regs[first]),m.reg_read(regs[first+1])))[0]
 def dbits(v):return struct.unpack('<Q',struct.pack('<d',v))[0]
 def retd(m,v):
  n=dbits(v);ret(m,n&0xffffffff,n>>32)
 for suffix,op in [('eq',lambda a,b:a==b),('lt',lambda a,b:a<b),('le',lambda a,b:a<=b),('gt',lambda a,b:a>b),('ge',lambda a,b:a>=b),('un',lambda a,b:math.isnan(a)or math.isnan(b))]:
  hook('__wrap___aeabi_dcmp'+suffix,lambda m,op=op:ret(m,int(op(d(m,0),d(m,2)))))
 for suffix,op in [('add',lambda a,b:a+b),('sub',lambda a,b:a-b),('mul',lambda a,b:a*b),('div',lambda a,b:a/b if b else math.copysign(math.inf,a))]:
  hook('__wrap___aeabi_d'+suffix,lambda m,op=op:retd(m,op(d(m,0),d(m,2))))
 hook('__wrap___aeabi_i2d',lambda m:retd(m,float(signed(m.reg_read(UC_ARM_REG_R0)))))
 hook('__wrap___aeabi_ui2d',lambda m:retd(m,float(m.reg_read(UC_ARM_REG_R0))))
 hook('__wrap___aeabi_f2d',lambda m:retd(m,f(m,UC_ARM_REG_R0)))
 hook('__wrap___aeabi_d2f',lambda m:ret(m,bits(d(m,0))))
 hook('__wrap___aeabi_d2iz',lambda m:ret(m,int(d(m,0))))
 hook('__wrap___aeabi_d2uiz',lambda m:ret(m,int(d(m,0))))
 hook('__wrap___ctzsi2',lambda m:ret(m,(m.reg_read(UC_ARM_REG_R0)&-m.reg_read(UC_ARM_REG_R0)).bit_length()-1 if m.reg_read(UC_ARM_REG_R0) else 32))
 hook('__wrap___clzsi2',lambda m:ret(m,32-m.reg_read(UC_ARM_REG_R0).bit_length()))
 def retlong(m,n):ret(m,n&0xffffffff,(n>>32)&0xffffffff)
 def longvalue(m):return m.reg_read(UC_ARM_REG_R0)|(m.reg_read(UC_ARM_REG_R1)<<32)
 hook('__wrap___aeabi_d2ulz',lambda m:retlong(m,int(d(m,0))))
 hook('__wrap___aeabi_d2lz',lambda m:retlong(m,int(d(m,0))))
 hook('__wrap___aeabi_ul2d',lambda m:retd(m,float(longvalue(m))))
 hook('__wrap___aeabi_l2d',lambda m:retd(m,float(longvalue(m) if longvalue(m)<2**63 else longvalue(m)-2**64)))
 return counts
