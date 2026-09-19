function parseUpdates(t){
  return t.split(/\n+/).map(x=>x.trim()).filter(Boolean).map(x=>{
    const p=x.split("|").map(y=>y.trim());
    return {package_type:"UPDATE",name:"Update "+(p[0]||""),version:p[0]||"1.00",source_url:p[1]||"",size_bytes:Number(p[2]||0),sha256:p[3]||"",required_base_version:p[4]||""};
  });
}
function parseDlcs(t){
  return t.split(/\n+/).map(x=>x.trim()).filter(Boolean).map(x=>{
    const p=x.split("|").map(y=>y.trim());
    return {package_type:"DLC",name:p[0]||"DLC",version:p[1]||"1.00",source_url:p[2]||"",size_bytes:Number(p[3]||0),sha256:p[4]||"",required_base_version:p[5]||""};
  });
}
async function findDbId(titleId){
  const d=await api("/api/titles");
  const arr=Array.isArray(d)?d:(d.titles||d.catalog||[]);
  const g=arr.find(x=>x.title_id===titleId);
  return g?Number(g.id):0;
}
async function saveGame(){
  const btn=document.getElementById("saveBtn"),msg=document.getElementById("saveMsg");
  const info={
    title_id:document.getElementById("fTitle").value.trim(),
    name:document.getElementById("fName").value.trim(),
    description:document.getElementById("fDesc").value.trim(),
    category:document.getElementById("fCat").value,
    region:document.getElementById("fRegion").value.trim(),
    cover_url:"",
    featured:document.getElementById("fFeatured").value==="true"
  };
  if(!info.title_id||!info.name){
    msg.innerHTML='<div class="note error">Nome e Title ID são obrigatórios.</div>';return;
  }
  btn.disabled=true;btn.textContent="Salvando...";
  try{
    const created=await api("/api/titles",{method:"POST",body:JSON.stringify(info)},true);
    let id=Number(created?.title?.id||created?.id||0);
    if(!id) id=await findDbId(info.title_id);
    if(!id) throw new Error("Não consegui localizar o ID criado.");
    const baseUrl=document.getElementById("fBaseUrl").value.trim();
    if(baseUrl){
      await api("/api/titles/"+id+"/packages",{method:"POST",body:JSON.stringify({
        package_type:"BASE",name:"Base",version:document.getElementById("fBaseVer").value.trim()||"1.00",
        required_base_version:"",source_url:baseUrl,size_bytes:Number(document.getElementById("fBaseSize").value||0),
        sha256:document.getElementById("fBaseSha").value.trim()
      })},true);
    }
    for(const p of parseUpdates(document.getElementById("fUpdates").value)){
      if(p.source_url) await api("/api/titles/"+id+"/packages",{method:"POST",body:JSON.stringify(p)},true);
    }
    for(const p of parseDlcs(document.getElementById("fDlcs").value)){
      if(p.source_url) await api("/api/titles/"+id+"/packages",{method:"POST",body:JSON.stringify(p)},true);
    }
    msg.innerHTML='<div class="note success">Cadastro concluído.</div>';
    await refreshCatalog();toast("Jogo salvo no Cloudflare.");setTimeout(()=>go("games"),450);
  }catch(e){
    msg.innerHTML='<div class="note error">'+esc(e.message)+'</div>';
  }finally{
    btn.disabled=false;btn.textContent="Salvar no Cloudflare";
  }
}
