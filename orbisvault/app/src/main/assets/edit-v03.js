async function editTitle(id){
  const g=catalog.catalog.find(x=>Number(x.id)===Number(id));if(!g)return;
  document.getElementById("sheet").innerHTML=
    '<h2>Editar título</h2><input id="eName" class="field" value="'+esc(g.name)+'" placeholder="Nome">'+
    '<input id="eTitle" class="field" value="'+esc(g.title_id)+'" placeholder="Title ID">'+
    '<textarea id="eDesc" class="field area">'+esc(g.description||"")+'</textarea>'+
    '<div class="row"><input id="eCat" class="field" value="'+esc(g.category||"PS4")+'" placeholder="Categoria"><input id="eRegion" class="field" value="'+esc(g.region||"")+'" placeholder="Região"></div>'+
    '<input id="eCover" class="field" value="'+esc(g.cover_url||"")+'" placeholder="URL da capa">'+
    '<button class="primary" onclick="saveTitleEdit('+id+')">Salvar alterações</button>';
}
async function saveTitleEdit(id){
  try{
    const old=catalog.catalog.find(x=>Number(x.id)===Number(id));
    const body={
      title_id:document.getElementById("eTitle").value.trim(),
      name:document.getElementById("eName").value.trim(),
      description:document.getElementById("eDesc").value.trim(),
      category:document.getElementById("eCat").value.trim(),
      region:document.getElementById("eRegion").value.trim(),
      cover_url:document.getElementById("eCover").value.trim(),
      featured:!!old?.featured
    };
    await api("/api/titles/"+id,{method:"PUT",body:JSON.stringify(body)},true);
    await refreshCatalog();closeModal();toast("Título atualizado.");render(currentView);
  }catch(e){toast(e.message)}
}
async function editPackage(id){
  const all=catalog.catalog.flatMap(g=>[...(g.base?[g.base]:[]),...(g.updates||[]),...(g.dlcs||[])]);
  const p=all.find(x=>Number(x.id)===Number(id));
  if(!p){toast("Package não encontrado.");return}
  document.getElementById("sheet").innerHTML=
    '<h2>Editar '+esc(p.package_type)+'</h2>'+
    '<input id="pName" class="field" value="'+esc(p.name||"")+'" placeholder="Nome">'+
    '<input id="pVer" class="field" value="'+esc(p.version||"")+'" placeholder="Versão">'+
    '<input id="pReq" class="field" value="'+esc(p.required_base_version||"")+'" placeholder="Versão Base exigida">'+
    '<input id="pUrl" class="field" value="'+esc(p.source_url||"")+'" placeholder="URL">'+
    '<input id="pSize" class="field" value="'+Number(p.size_bytes||0)+'" placeholder="Bytes">'+
    '<input id="pSha" class="field" value="'+esc(p.sha256||"")+'" placeholder="SHA-256">'+
    '<button class="primary" onclick="savePackageEdit('+id+',\''+esc(p.package_type)+'\')">Salvar package</button>';
}
async function savePackageEdit(id,type){
  try{
    const b={
      package_type:type,
      name:document.getElementById("pName").value.trim(),
      version:document.getElementById("pVer").value.trim(),
      required_base_version:document.getElementById("pReq").value.trim(),
      source_url:document.getElementById("pUrl").value.trim(),
      size_bytes:Number(document.getElementById("pSize").value||0),
      sha256:document.getElementById("pSha").value.trim()
    };
    await api("/api/packages/"+id,{method:"PUT",body:JSON.stringify(b)},true);
    await refreshCatalog();closeModal();toast("Package atualizado.");render(currentView);
  }catch(e){toast(e.message)}
}
