<?php
require_once("./config/config.php");

if(!isset($_POST['cmd']) || !isset($_POST['psd']) || !isset($_POST['passwd']))
	mDie(1,"La requ�te est incompl�te");
	
$cmd = secure($_POST['cmd']);
$psd = secure($_POST['psd']);
$passwd = md5($_POST['passwd']);

$req = "SELECT passwd FROM member WHERE pseudo='$psd'";

$sql = sqlConnect();
$resp = mysql_query($req);

if(mysql_num_rows($res) < 1)
	mDie(2,"Utilisateur non enregistr�");
	
$data = mysql_fetch_array($resp);

mysql_close($sql);

if(strcmp($data['passwd'],$passwd) != 0)
	mDie(3,"Nom d'utilisateur ou mot de passe incorrect");
	

// Sinon tout est bon on effectu l'op�ration correspondant � la commande pass�e.
if($cmd == 0)						// "Get partie" 
{
	// Requ�te sql de cr�ation de partie.
	$req = "...";
	
	$sql = sqlConnect();
	$resp = mysql_query($req);
	
	if(mysql_num_rows($resp) == 0)
		echo mysql_error();
	else
	{
		$sequence = "...";
		echo $sequence;
	}
	
	mysql_close($sql);
}
else if($cmd == 1)					// "Set partie"
{
	// Requ�te sql d'ajout d'informations (et calcul de r�sultat).
	$req = "...";
	
	$sql = sqlConnect();
	$resp = mysql_query($req);
	
	if(mysql_num_rows($resp) == 0)
		echo mysql_error();
	else
	{
		$sequence = "...";
		echo $sequence;
	}
	
	mysql_close($sql);
}
else if($cmd == 2)
{

}
else if($cmd == 3)
{

}
else if($cmd == 4)
{

}
else
	die("Commande inconnue");
	
?>