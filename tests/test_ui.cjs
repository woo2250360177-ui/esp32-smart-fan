const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/dashboard.html'),'utf8');
assert(!html.includes('\ufffd'));
const ids={},buttons=[];
function elem(id){return ids[id]||(ids[id]={textContent:'',disabled:false,attrs:{},handlers:{},style:{},classList:{add(){},remove(){}},setAttribute(k,v){this.attrs[k]=v},getAttribute(k){return this.attrs[k]},addEventListener(k,v){this.handlers[k]=v},getContext(){return new Proxy({},{get:(o,k)=>o[k]||(()=>{}),set:(o,k,v)=>(o[k]=v,true)})}});}
for(const m of html.matchAll(/\bid="([^"]+)"/g))elem(m[1]);
for(const m of html.matchAll(/<button\b([^>]+)>/g)){const a=m[1],id=/\bid="([^"]+)"/.exec(a),v=/\bvalue="([^"]+)"/.exec(a);const b=elem(id?id[1]:'b'+buttons.length);b.value=v[1];buttons.push(b);}
const ctx={document:{getElementById:id=>{assert(ids[id],id);return ids[id]},querySelectorAll:()=>buttons},XMLHttpRequest:function(){this.open=()=>{};this.send=()=>{}},setTimeout(){},confirm:()=>false};vm.createContext(ctx);vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],ctx);
const s={ms:1000,sampled:true,raw:25,filtered:24.8,predicted:24.8,rate:null,mode:0,level:1,pwm:128,alarms:0,minT:25,maxT:25,meanOutput:50.2,changes:0,count:1,experimentId:1,recordEndReason:'recording',firmware:'10.0.0-smallfan',recording:true,baseline:0,trend:0};
ctx.show({...s});assert(ids.temp.textContent.includes('25.00'));assert(ids.version.textContent.includes(s.firmware));
for(const b of buttons)if(['manual','threshold','trend','1','2','3','start','clear'].includes(b.value))assert(b.disabled);
assert(!ids.s0.disabled);assert.equal(ids.download.attrs['aria-disabled'],'true');
ctx.show({...s,recording:false});assert(!ids.m1.disabled);assert.equal(ids.download.attrs['aria-disabled'],'false');
ctx.show({...s,raw:null,filtered:null,predicted:null});assert.equal(ctx.points.at(-1).baseline,null);
for(let i=0;i<200;i++)ctx.show({...s,ms:2000+i*1000});assert.equal(ctx.points.length,180);
ctx.show({...s,ms:1});assert.equal(ctx.points.length,1);
ctx.fail();assert(ids.m1.disabled);assert(!ids.s0.disabled);assert.equal(ids.fan.className,'fan');
let cancelled=false;buttons.find(b=>b.value==='start').handlers.click({preventDefault(){cancelled=true}});assert(cancelled);
console.log('PASS: dashboard rendering, record locks, stop, CSV availability, invalid samples, history, reboot, offline and confirmation');
