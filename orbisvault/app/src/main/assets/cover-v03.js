function uploadCover(id,titleId){
  document.getElementById("sheet").innerHTML=
    '<h2>Trocar capa</h2><p class="sub">'+esc(titleId)+'</p>'+
    '<input id="coverFile" class="field" type="file" accept="image/jpeg,image/png,image/webp">'+
    '<button class="primary" onclick="doUploadCover('+id+',\''+esc(titleId)+'\')">Enviar para R2</button>';
}

async function doUploadCover(id,titleId){
  const f=document.getElementById("coverFile").files[0];
  if(!f){toast("Selecione uma imagem.");return}
  try{
    const fd=new FormData();
    fd.append("file",f);
    fd.append("title_id",titleId);
    const up=await api("/api/upload/cover",{method:"POST",body:fd},true);
    const g=catalog.catalog.find(x=>Number(x.id)===Number(id));
    if(up.url&&g){
      await api("/api/titles/"+id,{method:"PUT",body:JSON.stringify({
        title_id:g.title_id,
        name:g.name,
        description:g.description||"",
        category:g.category||"PS4",
        region:g.region||"",
        cover_url:up.url,
        featured:!!g.featured
      })},true);
    }
    await refreshCatalog();
    closeModal();
    toast("Capa atualizada.");
    render(currentView);
  }catch(e){
    toast(e.message);
  }
}
