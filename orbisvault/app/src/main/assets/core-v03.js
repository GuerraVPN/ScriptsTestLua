const API="https://orbis-vault-api.guerraf1000.workers.dev";
let catalog={revision:0,catalog:[]};
let currentView="home";

function esc(s){
  return String(s??"").replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
}
function token(){return sessionStorage.getItem("ov_token")||""}
function toast(t){
  const e=document.getElementById("toast");
  e.textContent=t;e.classList.add("show");
  setTimeout(()=>e.classList.remove("show"),2200);
}
function showLogin(){
  document.getElementById("splash").classList.add("hidden");
  document.getElementById("app").classList.add("hidden");
  document.getElementById("login").classList.remove("hidden");
}
function showApp(){
  document.getElementById("splash").classList.add("hidden");
  document.getElementById("login").classList.add("hidden");
  document.getElementById("app").classList.remove("hidden");
}
async function api(path,opt={},needAuth=false){
  const headers=new Headers(opt.headers||{});
  if(!(opt.body instanceof FormData)&&opt.body!==undefined&&!headers.has("Content-Type")){
    headers.set("Content-Type","application/json");
  }
  if(needAuth){
    if(!token()) throw new Error("Sessão expirada");
    headers.set("Authorization","Bearer "+token());
  }
  const r=await fetch(API+path,{...opt,headers});
  let data={};
  try{data=await r.json()}catch{}
  if(r.status===401){
    sessionStorage.removeItem("ov_token");
    throw new Error("Sessão expirada. Entre novamente.");
  }
  if(!r.ok) throw new Error(data.error||data.message||("Erro HTTP "+r.status));
  return data;
}
async function login(){
  const email=document.getElementById("loginEmail").value.trim();
  const password=document.getElementById("loginPass").value;
  const msg=document.getElementById("loginMsg");
  const btn=document.getElementById("loginBtn");
  if(!email||!password){
    msg.innerHTML='<div class="note error">Informe e-mail e senha.</div>';return;
  }
  btn.disabled=true;btn.textContent="Entrando...";
  try{
    const d=await api("/api/auth/login",{method:"POST",body:JSON.stringify({email,password})});
    if(!d.token) throw new Error("A API não retornou token.");
    sessionStorage.setItem("ov_token",d.token);
    sessionStorage.setItem("ov_email",email);
    document.getElementById("loginPass").value="";
    showApp();await refreshCatalog();render("home");
  }catch(e){
    msg.innerHTML='<div class="note error">'+esc(e.message)+'</div>';
  }finally{
    btn.disabled=false;btn.textContent="Entrar";
  }
}
function logout(){
  sessionStorage.clear();showLogin();toast("Sessão encerrada.");
}
async function refreshCatalog(){
  try{
    const d=await api("/api/catalog");
    catalog={revision:Number(d.revision||0),catalog:Array.isArray(d.catalog)?d.catalog:[]};
    localStorage.setItem("ov_catalog_cache",JSON.stringify(catalog));
    return true;
  }catch(e){
    try{
      const c=JSON.parse(localStorage.getItem("ov_catalog_cache")||"null");
      if(c&&Array.isArray(c.catalog)) catalog=c;
    }catch{}
    return false;
  }
}
function setNav(v){
  document.querySelectorAll(".nav").forEach(n=>n.classList.toggle("active",n.dataset.view===v));
}
async function go(v){
  currentView=v;setNav(v);
  if(v!=="add") await refreshCatalog();
  render(v);
}
function closeModal(){document.getElementById("modal").classList.add("hidden")}
setTimeout(async()=>{
  if(token()){
    showApp();await refreshCatalog();render("home");
  }else{
    showLogin();
  }
},850);
