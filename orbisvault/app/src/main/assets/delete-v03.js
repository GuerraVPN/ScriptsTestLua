async function deletePackage(id){
  if(!confirm("Excluir este package?"))return;
  try{
    await api("/api/packages/"+id,{method:"DELETE"},true);
    await refreshCatalog();
    closeModal();
    toast("Package excluído.");
    render(currentView);
  }catch(e){
    toast(e.message);
  }
}

async function deleteTitle(id){
  if(!confirm("Excluir este título e todos os packages relacionados?"))return;
  try{
    await api("/api/titles/"+id,{method:"DELETE"},true);
    await refreshCatalog();
    closeModal();
    toast("Título excluído.");
    render("games");
  }catch(e){
    toast(e.message);
  }
}
