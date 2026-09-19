let ovDevices=[];

async function loadDevices(){
  const d=await api("/api/admin/devices",{},true);
  ovDevices=Array.isArray(d.devices)?d.devices:[];
  return ovDevices;
}

function deviceStateText(d){
  if(!d.enabled)return "Desativado";
  if(!d.paired)return "Aguardando pareamento";
  return d.last_seen?("Ultimo contato: "+d.last_seen):"Pareado";
}

async function showConsoleManager(){
  document.getElementById("modal").classList.remove("hidden");
  document.getElementById("sheet").innerHTML=
    '<h2>Consoles PS4</h2><p class="sub">Pareie o Orbis Vault do PS4 para instalar remotamente.</p>'+
    '<div class="loading">Consultando...</div>';
  try{
    const list=await loadDevices();
    let html='<h2>Consoles PS4</h2><p class="sub">Comandos passam pelo Cloudflare.</p>'+
      '<button class="primary" onclick="pairConsole()">Parear novo PS4</button><div style="height:12px"></div>';
    if(!list.length){
      html+='<div class="note">Nenhum PS4 pareado ainda. Abra o Orbis Vault no console, escolha Parear e informe aqui o codigo exibido na TV.</div>';
    }else{
      html+=list.map(d=>
        '<div class="menuitem" onclick="showRemoteJobs(\''+esc(d.device_id)+'\')"><div class="mi">🎮</div><div class="mt"><b>'+esc(d.device_name||"PS4")+'</b>'+
        '<small>'+esc(d.device_id)+'</small><small class="'+(d.paired&&d.enabled?'green':'muted')+'">'+esc(deviceStateText(d))+'</small></div><div>›</div></div>'
      ).join('');
    }
    document.getElementById("sheet").innerHTML=html;
  }catch(e){
    document.getElementById("sheet").innerHTML=
      '<h2>Consoles PS4</h2><div class="note error">'+esc(e.message)+'</div>'+
      '<div class="note">A API remota ainda precisa estar publicada no Worker.</div>';
  }
}

async function pairConsole(){
  const code=prompt("Digite o codigo de 6 digitos mostrado no PS4:");
  if(!code)return;
  if(!/^\d{6}$/.test(code.trim())){
    toast("O codigo precisa ter 6 digitos.");
    return;
  }
  try{
    const d=await api("/api/admin/devices/pair/confirm",{
      method:"POST",
      body:JSON.stringify({code:code.trim()})
    },true);
    toast(d.message||"PS4 pareado.");
    await showConsoleManager();
  }catch(e){
    toast(e.message);
  }
}

async function installOnPs4(titleId){
  const game=catalog.catalog.find(x=>Number(x.id)===Number(titleId));
  if(!game){toast("Titulo nao encontrado.");return}

  try{
    const list=await loadDevices();
    const ready=list.filter(d=>d.paired&&d.enabled);
    if(!ready.length){
      toast("Nenhum PS4 pareado.");
      showConsoleManager();
      return;
    }

    if(ready.length===1){
      await sendInstallCommand(ready[0],game);
      return;
    }

    document.getElementById("sheet").innerHTML=
      '<h2>Instalar no PS4</h2><p class="sub">'+esc(game.name)+'</p>'+
      ready.map(d=>
        '<button class="secondary" style="margin-bottom:8px" onclick="sendInstallById(\''+
        esc(d.device_id)+'\','+Number(game.id)+')">'+esc(d.device_name||d.device_id)+'</button>'
      ).join('');
  }catch(e){
    toast(e.message);
  }
}

async function sendInstallById(deviceId,titleDbId){
  const game=catalog.catalog.find(x=>Number(x.id)===Number(titleDbId));
  const device=ovDevices.find(d=>d.device_id===deviceId);
  if(game&&device)await sendInstallCommand(device,game);
}

async function sendInstallCommand(device,game){
  try{
    const d=await api("/api/admin/device/commands",{
      method:"POST",
      body:JSON.stringify({
        device_id:device.device_id,
        action:"INSTALL_ALL",
        title_db_id:Number(game.id),
        title_id:game.title_id,
        package_ids:[]
      })
    },true);
    toast("Instalacao enviada para "+(device.device_name||"PS4")+".");
    await showRemoteJobs(device.device_id);
    return d;
  }catch(e){
    toast(e.message);
  }
}


let remoteStatusTimer=null;

function remoteStatusClass(status){
  if(status==="COMPLETED")return "green";
  if(status==="ERROR"||status==="CANCELLED")return "error";
  return "blue";
}

function remoteCommandLabel(c){
  if(c.action==="INSTALL_ALL")return "Instalar tudo";
  if(c.action==="INSTALL_SELECTED")return "Instalar selecionados";
  if(c.action==="SYNC_CATALOG")return "Sincronizar catálogo";
  if(c.action==="CANCEL_COMMAND")return "Cancelar";
  return c.action||"Comando";
}

async function showRemoteJobs(deviceId=""){
  if(remoteStatusTimer){clearInterval(remoteStatusTimer);remoteStatusTimer=null}
  document.getElementById("modal").classList.remove("hidden");
  document.getElementById("sheet").innerHTML=
    '<h2>Instalações remotas</h2><p class="sub">Consultando o PS4...</p>'+
    '<div class="loading">Atualizando...</div>';
  await refreshRemoteJobs(deviceId);
  remoteStatusTimer=setInterval(()=>refreshRemoteJobs(deviceId),4000);
}

async function refreshRemoteJobs(deviceId=""){
  const modal=document.getElementById("modal");
  if(!modal||modal.classList.contains("hidden")){
    if(remoteStatusTimer){clearInterval(remoteStatusTimer);remoteStatusTimer=null}
    return;
  }
  try{
    const qs=deviceId?("?device_id="+encodeURIComponent(deviceId)+"&limit=20"):"?limit=20";
    const d=await api("/api/admin/device/commands"+qs,{},true);
    const arr=Array.isArray(d.commands)?d.commands:[];
    let html='<div style="display:flex;justify-content:space-between;align-items:center"><h2 style="margin:0">Instalações remotas</h2><button class="iconbtn" onclick="closeModal()">✕</button></div>'+
      '<p class="sub">Atualização automática a cada 4 segundos.</p>';
    if(!arr.length){
      html+='<div class="note">Nenhum comando remoto encontrado.</div>';
    }else{
      html+=arr.map(c=>{
        const p=Math.max(0,Math.min(100,Number(c.progress||0)));
        const title=c.title_id||("Título #"+(c.title_db_id||"-"));
        return '<div class="card" style="margin:10px 0">'+
          '<div class="kv"><span>'+esc(remoteCommandLabel(c))+'</span><b class="'+remoteStatusClass(c.status)+'">'+esc(c.status||"-")+'</b></div>'+
          '<div class="kv"><span>'+esc(title)+'</span><b>'+p+'%</b></div>'+
          '<div style="height:8px;background:#08182b;border-radius:999px;overflow:hidden;margin-top:10px"><div style="height:100%;width:'+p+'%;background:#168cff"></div></div>'+
          (c.message?'<small class="muted" style="display:block;margin-top:9px">'+esc(c.message)+'</small>':'')+
          (["QUEUED","ACCEPTED","DOWNLOADING","VERIFYING","INSTALLING"].includes(c.status)
            ?'<button class="dangerBtn" style="margin-top:10px" onclick="cancelRemoteCommand('+Number(c.id)+',\''+esc(c.device_id||deviceId)+'\')">Cancelar</button>'
            :'')+
          '</div>';
      }).join('');
    }
    document.getElementById("sheet").innerHTML=html;
  }catch(e){
    document.getElementById("sheet").innerHTML=
      '<div style="display:flex;justify-content:space-between;align-items:center"><h2 style="margin:0">Instalações remotas</h2><button class="iconbtn" onclick="closeModal()">✕</button></div>'+
      '<div class="note error">'+esc(e.message)+'</div>';
  }
}


async function cancelRemoteCommand(commandId,deviceId=""){
  try{
    const d=await api("/api/admin/device/commands/"+Number(commandId)+"/cancel",{
      method:"POST",
      body:JSON.stringify({})
    },true);
    toast(d.message||"Cancelamento solicitado.");
    await refreshRemoteJobs(deviceId);
  }catch(e){
    toast(e.message);
  }
}
