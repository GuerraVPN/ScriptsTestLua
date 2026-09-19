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
        '<div class="menuitem"><div class="mi">🎮</div><div class="mt"><b>'+esc(d.device_name||"PS4")+'</b>'+
        '<small>'+esc(d.device_id)+'</small><small class="'+(d.paired&&d.enabled?'green':'muted')+'">'+esc(deviceStateText(d))+'</small></div></div>'
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
    closeModal();
    toast("Instalacao enviada para "+(device.device_name||"PS4")+".");
    return d;
  }catch(e){
    toast(e.message);
  }
}
