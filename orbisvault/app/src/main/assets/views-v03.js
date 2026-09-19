function stats(){
  let u=0,d=0;
  catalog.catalog.forEach(x=>{u+=(x.updates||[]).length;d+=(x.dlcs||[]).length});
  return {g:catalog.catalog.length,u,d};
}
function coverHtml(g){
  if(g.cover_url){
    return '<div class="cover"><img src="'+esc(g.cover_url)+'" onerror="this.parentNode.innerHTML=\'🎮\'"></div>';
  }
  return '<div class="cover">🎮</div>';
}
function gameRow(g){
  return '<div class="game" onclick="details('+Number(g.id)+')">'+coverHtml(g)+
    '<div class="game-info"><b>'+esc(g.name)+'</b><small>'+esc(g.title_id)+' • '+(g.base?('v'+esc(g.base.version)):'sem base')+
    '</small><small>'+esc(g.category||"PS4")+' • '+esc(g.region||"-")+'</small></div><i class="dot"></i></div>';
}
function render(v){
  const el=document.getElementById("view"),s=stats();
  if(v==="home"){
    el.innerHTML='<h2>Olá 👋</h2><p class="sub">Catálogo conectado ao Cloudflare.</p>'+
      '<div class="grid">'+
      '<div class="card stat"><small>Jogos</small><strong>'+s.g+'</strong></div>'+
      '<div class="card stat"><small>Updates</small><strong>'+s.u+'</strong></div>'+
      '<div class="card stat"><small>DLCs</small><strong>'+s.d+'</strong></div>'+
      '<div class="card stat"><small>Revision</small><strong>'+catalog.revision+'</strong></div></div>'+
      '<div class="section-title"><b>Catálogo</b><span class="link" onclick="go(\'games\')">Ver todos</span></div>'+
      (catalog.catalog.length?catalog.catalog.slice(0,6).map(gameRow).join(''):'<div class="note">O catálogo está vazio. Use <b>Adicionar</b> para cadastrar o primeiro item.</div>');
  }else if(v==="games"){
    el.innerHTML='<h2>Lista de Jogos</h2><p class="sub">Dados carregados diretamente do Cloudflare.</p>'+
      '<input id="q" class="field" placeholder="Pesquisar jogos..." oninput="filterGames()">'+
      '<div class="chips"><button class="chip active">Todos</button><button class="chip">PS4</button><button class="chip">Homebrew</button><button class="chip">Utilitário</button></div>'+
      '<div id="gamesList">'+catalog.catalog.map(gameRow).join('')+'</div>';
  }else if(v==="add"){
    renderAdd();
  }else if(v==="sync"){
    renderSync();
  }else{
    renderMore();
  }
}
function filterGames(){
  const q=document.getElementById("q").value.toLowerCase();
  const arr=catalog.catalog.filter(g=>(g.name+" "+g.title_id+" "+(g.category||"")+" "+(g.region||"")).toLowerCase().includes(q));
  document.getElementById("gamesList").innerHTML=arr.map(gameRow).join('')||'<div class="note">Nenhum resultado.</div>';
}
function renderAdd(){
  const el=document.getElementById("view");
  el.innerHTML='<h2>Adicionar Jogo</h2><p class="sub">Cadastre título, capa, Base, Updates e DLCs no Cloudflare.</p>'+
    '<div class="form-card">'+
    '<label class="label">Nome</label><input id="fName" class="field" placeholder="Nome do jogo">'+
    '<div class="row"><div><label class="label">Title ID</label><input id="fTitle" class="field" placeholder="BREW00001"></div><div><label class="label">Região</label><input id="fRegion" class="field" placeholder="USA"></div></div>'+
    '<div class="row"><div><label class="label">Categoria</label><select id="fCat" class="field"><option>PS4</option><option>Homebrew</option><option>Utilitário</option><option>Emulador</option></select></div><div><label class="label">Destaque</label><select id="fFeatured" class="field"><option value="false">Não</option><option value="true">Sim</option></select></div></div>'+
    '<label class="label">Descrição</label><textarea id="fDesc" class="field area" placeholder="Descrição..."></textarea>'+
    '<label class="label">Capa</label><input id="fCover" class="field" type="file" accept="image/jpeg,image/png,image/webp">'+
    '<div class="section-title"><b>BASE</b></div><div class="row"><input id="fBaseVer" class="field" value="1.00" placeholder="Versão"><input id="fBaseSize" class="field" inputmode="numeric" placeholder="Bytes"></div>'+
    '<input id="fBaseUrl" class="field" placeholder="Link da Base"><input id="fBaseSha" class="field" placeholder="SHA-256 (opcional)">'+
    '<div class="section-title"><b>UPDATES</b><span class="muted">1 por linha</span></div>'+
    '<textarea id="fUpdates" class="field area" placeholder="1.01 | https://... | 10485760 | sha256 | 1.00"></textarea>'+
    '<div class="section-title"><b>DLCs</b><span class="muted">1 por linha</span></div>'+
    '<textarea id="fDlcs" class="field area" placeholder="Nome DLC | 1.00 | https://... | 2097152 | sha256 | 1.00"></textarea>'+
    '<button id="saveBtn" class="primary" onclick="saveGame()">Salvar no Cloudflare</button><div id="saveMsg"></div></div>';
}
async function details(id){
  let g=catalog.catalog.find(x=>Number(x.id)===Number(id));
  if(!g)return;
  try{
    const d=await api("/api/titles/"+id);
    g=d.title||d;
  }catch{}
  const packages=[...(g.base?[g.base]:[]),...(g.updates||[]),...(g.dlcs||[])];
  document.getElementById("sheet").innerHTML=
    '<div style="display:flex;justify-content:space-between;align-items:center"><h2 style="margin:0">'+esc(g.name)+'</h2><button class="iconbtn" onclick="closeModal()">✕</button></div>'+
    '<p class="sub">'+esc(g.title_id)+' • '+esc(g.category||"PS4")+' • '+esc(g.region||"-")+'</p>'+
    '<div class="card"><div class="kv"><span>Revision atual</span><b class="blue">'+catalog.revision+'</b></div>'+
    '<div class="kv"><span>Base</span><b class="green">'+(g.base?('v'+esc(g.base.version)):'Não cadastrada')+'</b></div>'+
    '<div class="kv"><span>Updates</span><b>'+((g.updates||[]).length)+'</b></div><div class="kv"><span>DLCs</span><b>'+((g.dlcs||[]).length)+'</b></div></div>'+
    '<div class="section-title"><b>Pacotes</b></div>'+packages.map(packageRow).join('')+
    (packages.length?'':'<div class="note">Nenhum package cadastrado.</div>')+
    '<div class="section-title"><b>Administrar</b></div>'+
    '<button class="secondary" onclick="editTitle('+Number(g.id)+')">Editar informações</button><div style="height:8px"></div>'+
    '<button class="secondary" onclick="uploadCover('+Number(g.id)+',\''+esc(g.title_id)+'\')">Trocar capa</button><div style="height:8px"></div>'+
    '<button class="dangerBtn" onclick="deleteTitle('+Number(g.id)+')">Excluir título</button>';
  document.getElementById("modal").classList.remove("hidden");
}
function packageRow(p){
  const icon=p.package_type==="BASE"?"📦":p.package_type==="UPDATE"?"🔄":"🧩";
  return '<div class="package"><div>'+icon+'</div><div class="meta"><b>'+esc(p.package_type)+' • '+esc(p.name||"")+' • v'+esc(p.version)+'</b><small>'+esc(p.source_url||"Sem URL")+'</small></div>'+
    '<button class="iconbtn" onclick="event.stopPropagation();editPackage('+Number(p.id)+')">✎</button>'+
    '<button class="iconbtn trash" onclick="event.stopPropagation();deletePackage('+Number(p.id)+')">🗑</button></div>';
}
async function renderSync(){
  const el=document.getElementById("view");
  el.innerHTML='<h2>Sincronizar</h2><p class="sub">Consultando Cloudflare...</p>';
  let online=false,rev=catalog.revision,apiVersion="-";
  try{
    const d=await api("/api/catalog/version");
    online=true;rev=Number(d.revision||rev);apiVersion=d.version||d.api_version||"-";
  }catch{}
  el.innerHTML='<h2>Sincronizar</h2><p class="sub">O PS4 consulta o mesmo catálogo público.</p>'+
    '<div class="status"><div class="statusline"><div style="font-size:28px">☁️</div><div style="flex:1"><b>Cloudflare API</b><div class="muted">'+esc(API.replace("https://",""))+'</div></div><span class="pill '+(online?'ok':'off')+'">'+(online?'Online':'Offline')+'</span></div></div>'+
    '<div class="status"><div class="statusline"><div style="font-size:28px">🔢</div><div style="flex:1"><b>Revision</b><div class="muted">Versão API: '+esc(apiVersion)+'</div></div><span class="pill ok">'+rev+'</span></div></div>'+
    '<button class="primary" onclick="manualSync()">Atualizar catálogo agora</button>'+
    '<div class="note">As alterações feitas pelo Admin são gravadas diretamente no Cloudflare.</div>';
}
async function manualSync(){
  const ok=await refreshCatalog();
  toast(ok?"Catálogo sincronizado.":"Sem conexão; usando cache.");
  renderSync();
}
function renderMore(){
  document.getElementById("view").innerHTML='<h2>Mais</h2><p class="sub">Configurações e informações.</p>'+
    menu("👤","Conta",sessionStorage.getItem("ov_email")||"Administrador","Toque para sair","logout()")+
    menu("☁️","Servidor","Cloudflare Worker",API,"toast(\'API conectada ao Orbis Vault.\')")+
    menu("💾","Cache","Catálogo local de contingência","Revision "+catalog.revision,"toast(\'O cache é usado quando a API estiver indisponível.\')")+
    menu("ℹ️","Sobre","Orbis Vault Admin","v0.3.0 • build 3","toast(\'Orbis Vault Admin v0.3.0\')");
}
function menu(i,t,s,r,a){
  return '<div class="menuitem" onclick="'+a+'"><div class="mi">'+i+'</div><div class="mt"><b>'+esc(t)+'</b><small>'+esc(s)+'</small><small class="blue">'+esc(r)+'</small></div><div>›</div></div>';
}
