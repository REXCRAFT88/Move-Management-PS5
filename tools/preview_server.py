from http.server import BaseHTTPRequestHandler,HTTPServer
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
bits=[1,8,65536,1048576,256,16384,8192,32768,4096,16,32,64,128,1024,2048,2,4]
mapped=bits.copy();mapped[3]=mapped[5]=1024;mapped[4]=512
config={'combined':False,'virtual_status':'Emulator bridge','players':[{'preset':1,'trigger':2,'nav_axis':1,'move':mapped,'nav':bits} for _ in range(4)]}
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path=='/api/config': body=json.dumps(config).encode();kind='application/json'
  elif self.path=='/api/state':body=json.dumps({'version':'0.1.0-beta preview','running':True,'bluetooth':'ok','reason':'','pads':[{'pid':'03d5','addr':'02:00:00:00:00:01','buttons':['l2']},{'pid':'042f','addr':'02:00:00:00:00:02','buttons':[]}],'tracking':[{'player':1,'valid':True,'x':150,'y':125,'width':20,'height':22}]}).encode();kind='application/json'
  elif self.path=='/api/log':body=b'Move Management preview. No console actions.';kind='text/plain'
  else: body=(root/'web/index.html').read_bytes();kind='text/html; charset=utf-8'
  self.send_response(200);self.send_header('Content-Type',kind);self.end_headers();self.wfile.write(body)
 def do_POST(self):self.send_error(405,'Read-only preview')
 def log_message(self,*args):pass
HTTPServer(('127.0.0.1',8096),Handler).serve_forever()