import sys, numpy as np
from PIL import Image
def comp(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(np.float32)/255
    paper = np.array([246,239,217],np.float32)/255
    ink = np.array([20,18,15],np.float32)/255
    gold = np.array([176,138,60],np.float32)/255
    mark = np.array([28,96,70],np.float32)/255
    c = np.ones(a.shape,np.float32)*paper
    for ch,col in ((1,gold),(2,mark),(0,ink)):
        k=a[...,ch:ch+1]; c = c*(1-k)+col*k
    return Image.fromarray((c*255).astype(np.uint8))
ims=[comp(p) for p in sys.argv[2:]]
w=sum(i.width for i in ims); out=Image.new('RGB',(w,ims[0].height))
x=0
for i in ims[::-1]: out.paste(i,(x,0)); x+=i.width
out.save(sys.argv[1])
