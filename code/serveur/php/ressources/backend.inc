<?php
require_once("ressources/relations.inc");
require_once("ressources/db.inc");



/* Les prototypes des fonctions :
*===============================>
*	getGidFromPgid($user, $pgid);
*	checkLogin($user, $passwd);
*	randomCenterNode();
*	randomCloudNode();
*	cgBuildResultSets($cloudSize, $centerEid, $r1, $r2);
*	cgChooseRelations();
*	cgBuildCloud($centerEid, $cloudSize, $sources, $sumWeights);
*	insertCreatedGame($centerEid, $cloud, $r1, $r2, $totalDifficulty, $userName);
*	randomGameCore();
*	randomGame();
*	formatWord($word);
*	game2array($user, $pgid);
*	createGame($nbParties, $mode);
*	createGameCore($cloudSize);
*	computeScore($probas, $difficulty, $answer, $userReputation);
*	computeUserReputation($score);
*	normalizeProbas($row);
*	setGame($user, $pgid, $gid, $answers);
*	getGameRelations();
*	setGameGetScore($user, $pgid, $gid, $answers);
*	getNodeEid($node);
*	wordExist($node);
*	setUserInfo($user, $key, $value);
*	userPrefsDefaults();
*	userPrefs($user);
*	setUserPref($user, $key, $value);
*	getJAimePgid($user, $pgid);
*	setJAimePgid($user, $pgid, $value);
* 	addGameCreationBonus($user, $nb);
* function deleteOneGameCreation($user);
* 	getNbGameCreationRemained($user);
*/


/* Les définitions
====================*/

function longStrVal($str) {
	return preg_match('/^-?[0-9]+$/', ''.$str) ? ''.$str : '0';
}

/**  Vérifie la validité du couple nom d'utilisateur / mot de passe.
* @param user : Le nom d'utilisateur.
* @param passwd : Le mot de passe.
* @return boolean : true si OK sinon false.
*/
function checkLogin($user, $passwd) {
	if(isset($_SESSION['userId']))
		return true;
	connect($user, $passwd);
}

function connect($user, $passwd) {
	$db = getDB();
	$hashPasswd = md5($passwd);
	$loginIsOk = ($hashPasswd == $db->querySingle("SELECT hash_passwd FROM user WHERE login='".$user."';"));
	return $loginIsOk;
}

/** Selectionne aléatoirement l'eid d'un mot central.
* @return eid : Identifiant d'un mot central, NULL en cas d'erreur.
*/
function randomCenterNode()
{
	$db = getDB();
	return $db->querySingle("select eid from random_center_node where rowid = (abs(random()) % (select max(rowid) from random_center_node))+1;");
}

/** Selectionne aléatoirement un noeud d'un nuage.
* @return eid : L'identifiant du noeud.
*/
function randomCloudNode()
{
	$db = getDB();
	return $db->querySingle("select eid from random_cloud_node where rowid = (abs(random()) % (select max(rowid) from random_cloud_node))+1;");
}

/**
* @param cloudSize : Taille du nuage.
* @param centerEid : Identifiant du mot central.
* @param r1 Type de la relation 1.
* @param r2 Type de la relation 2.
*/
function cgBuildResultSets($cloudSize, $centerEid, $r1, $r2)
{
	$db = getDB();
	// 'w' => weight (poids), 'd' => difficulté, 's' => select
	// Le select doit ranvoyer trois colonnes :
	//   eid => l'eid du mot à mettre dans le nuage,
	//   r1 => la probabilité pour que le mot soit dans r1, entre -1 et 1 (négatif = ne devrait pas y être, positif = devrait y être à coup sûr, 0 = on sait pas).
	$typer1r2 = "type in ($r1, $r2)";
	$banned_types = "4, 12, 36, 18, 29, 45, 46, 47, 48, 1000, 1001";
	
	$sources = array(
		// Voisins 1 saut du bon type (= relations déjà existantes)
		array('w'=>40, 'd'=>1, 's'=>"select end as eid, type = $r1 as r1, type = $r2 as r2, 0 as r0, 0 as trash from relation where start = $centerEid and $typer1r2 order by random();"),
		// Voisins 1 saut via r_associated (0), donc qu'on voudrait spécifier si possible.
		array('w'=>40, 'd'=>2, 's'=>"select end as eid, 0.25 as r1, 0.25 as r2, 0.5 as r0, 0 as trash from relation where start = $centerEid and type = 0 order by random();"),
		// Voisins 1 saut via les autres relations
		array('w'=>20, 'd'=>3.1, 's'=>"select end as eid, 0.1 as r1, 0.1 as r2, 0.8 as r0, 0 as trash from relation where start = $centerEid and type not in (0, $r1, $r2, $banned_types) order by random();"),
		// Voisins 2 sauts, avec un mix de R1 et R2 pour les liens. Par ex [ A -R1-> B -R2-> C ] ou bien [ A -R2-> B -R2-> C ]
		// Version optimisée de : "select end as eid from relation where $typer1r2 and start in oneHopWithType order by random();"
		array('w'=>30, 'd'=>3.2, 's'=>"select B.end as eid, ((A.type = $r1) + (B.type = $r1)) / 3. as r1, ((A.type = $r2) + (B.type = $r2)) / 3. as r2, 1/6. as r0, 1/6. as trash from relation as A, relation as B where A.start = $centerEid and A.$typer1r2 and B.start = A.end and B.$typer1r2 order by random();"),
		// Voisins 1 saut r1/r2 + 1 saut synonyme
		// Version optimisée de : "select end as eid from relation where start in oneHopWithType and type = 5 order by random()";
		array('w'=>20, 'd'=>5, 's'=>"select B.end as eid, (A.type = $r1) * 0.75 as r1, (A.type = $r2) * 0.75 as r2, 0.25 as r0, 0 as trash from relation as A, relation as B where A.start = $centerEid and A.$typer1r2 and B.start = A.end and B.type = 5 order by random();"),
		// Version optimisée de : "select end as eid from relation where start in (select end from relation where start = $centerEid and type = 5) and $typer1r2 order by random();"
		array('w'=>20, 'd'=>6, 's'=>"select B.end as eid, (B.type = $r1) * 0.75 as r1, (B.type = $r2) * 0.75 as r2, 0.25 as r0, 0 as trash from relation as A, relation as B where A.start = $centerEid and A.type = 5 and B.start = A.end and B.$typer1r2 order by random();"),
		// Voisins 2 sauts (tous)
		// Version optimisée de : "select end as eid, 0.1 as r1, 0.1 as r2, 0.3 as r0, 0.5 as trash from relation where start in (select end from relation where start = $centerEid and type not in ($banned_types)) and type not in ($banned_types) order by random();"
		array('w'=>10, 'd'=>8, 's'=>"select x as eid, 0.1 as r1, 0.1 as r2, 0.3 as r0, 0.5 as trash from (select x from (select X.eid + Y.dumb as x from (select B.end as eid from relation as A, relation as B where A.type not in ($banned_types) and A.start = $centerEid and B.type not in ($banned_types) and B.start = A.end limit ".($cloudSize*4).") as X, (select 0 as dumb) as Y)) order by random();"),
		// Centre pointe vers X, M pointe vers X aussi, on prend M.
		// Version optimisée de : "select start as eid from relation where end in (select end from relation where start = $centerEid) and type not in ($banned_types) order by random();"
		// Ce n'est toujours pas ça… : "select eid from (select B.start as eid from relation as A, relation as B where A.type not in ($banned_types) and A.start = $centerEid and B.type not in ($banned_types) and B.end = A.end limit 1) order by random();"
		// Tordu, mais ça marche \o/ . En fait il faut empêcher l'optimiseur de ramener le random avant le limit (et l'optimiseur est malin… :)
		array('w'=>10, 'd'=>8, 's'=>"select x as eid, 0.1 as r1, 0.1 as r2, 0.2 as r0, 0.6 as trash from (select x from (select X.eid + Y.dumb as x from (select B.start as eid from relation as A, relation as B where A.type not in ($banned_types) and A.start = $centerEid and B.type not in ($banned_types) and B.end = A.end limit ".($cloudSize*4).") as X, (select 0 as dumb) as Y)) order by random();"),
		'rand' => array('w'=>5, 'd'=>10, 's'=>false) // random. Le r1 et r2 de random sont juste en-dessous
	);

	$sumWeights = 0;
	
	foreach ($sources as $k => $x)
	{
		$sumWeights += $x['w'];
		$sources[$k]['rsPos'] = 0;
		$sources[$k]['rsSize'] = 0;
		
		if ($x['s'] !== false)
		{
			$sources[$k]['resultSet'] = array();
			$res = $db->query($x['s']);
			$i = 0;
			
			while ($i < $cloudSize && $sources[$k]['resultSet'][] = $res->fetchArray())
			{
				$i++;
				$sources[$k]['rsSize']++;
			}
		} 
		else
		{
			$sources[$k]['resultSet'] = array();
			
			for ($i = 0; $i < $cloudSize; $i++)
			{
				$sources[$k]['resultSet'][] = array('eid'=>randomCloudNode(), 'r1'=>0, 'r2'=>0, 'r0'=>0, 'trash'=>1);
				$sources[$k]['rsSize']++;
			}
		}
	}

	return array($sources, $sumWeights);
}


/** Sélectionne aléatoirement deux relations.
* @return array : Tableau avec la relation 1 et la relation 2.
*/
function cgChooseRelations()
{
	global $stringRelations;
	$relations = array_values(array_diff(array_keys($stringRelations), array(-1, 0)));
	$r1 = rand(0,count($relations)-1);
	$r2 = rand(0,count($relations)-2);

	if ($r2 >= $r1)
		$r2++;

	$r1 = $relations[$r1];
	$r2 = $relations[$r2];

	return array($r1, $r2);
}

/** Génération d'un nuage pour un mot central.
* @param cloudSize : Taille du nuage.
* @param sources Les sources.
* @param sumWeights La somme des poids.
* @return array : Tableau avec comme premier élément le nuage et comme second élément le total de difficulté.
*/
function cgBuildCloud($centerEid, $cloudSize, $sources, $sumWeights)
{
	$db = getDB();
	// On boucle tant qu'il n'y a pas eu au moins 2 sources épuisées
	$cloud = array();
	$nbFailed = 0;
	$i = 0;
	$totalDifficulty = 0;
	
	while ($i < $cloudSize && $nbFailed < 10*$cloudSize)
	{
		// On choisit une source aléatoire en tennant compte des poids.
		$rands = rand(1,$sumWeights);
		$sumw = 0;
		if (!isset($sources['rand'])) {
			break;
		}
		$src = $sources['rand'];
		$srck = 'rand';

		// Sélection d'une source aléatoire
		foreach ($sources as $k => $x)
		{
			$sumw += $x['w'];
			
			if ($rands < $sumw)
			{
				$src = $x;
				$srck = $k;
				break;
			}
		}
		
		// Vérification si on est à la fin du ResultSet de cette source.
		if ($src['rsPos'] >= $src['rsSize'])
		{
			$nbFailed++;

			$sumWeights -= $src['w'];
			unset($sources[$srck]);

			continue;
		}
		
		// On récupère un résultat de cette source.
		$res = $src['resultSet'][$src['rsPos']];
		$sources[$srck]['rsPos']++;

		// On vérifie si le mot n'a pas déjà été sélectionné.
		$rejected = false;
		// Ne pas mettre le mot central dans le nuage.
		if ($res['eid'] == $centerEid) { continue; }
		$nodeName = $db->querySingle("select name from node where eid=".$res['eid'].";");
		if (substr($nodeName, 0, 2) == "::") { continue; }
		foreach ($cloud as $c) {
			if ($c['eid'] == $res['eid']) {
				$nbFailed++;
				$rejected = true;
				break;
			}
		}
		if ($rejected) { continue; }

		// position dans le nuage, difficulté, eid, probaR1, probaR2
		$totalDifficulty += $src['d'];
		$cloud[$i] = array('pos'=>$i++, 'd'=>$src['d'], 'eid'=>$res['eid'], 'probaR1'=>$res['r1'], 'probaR2'=>$res['r2'], 'probaR0'=>$res['r0'], 'probaTrash'=>$res['trash']);
	}

	$res = $sources['rand']['resultSet'][0];
	
	while ($i < $cloudSize)
	{
		$totalDifficulty += $sources['rand']['d'];
		$cloud[$i] = array('pos'=>$i++, 'd'=>$sources['rand']['d'], 'eid'=>randomCloudNode(), 'probaR1'=>$res['r1'], 'probaR2'=>$res['r2'], 'probaR0'=>$res['r0'], 'probaTrash'=>$res['trash']);
	}

	return array($cloud, $totalDifficulty);
}


function getSequencePlayedGame() {
	$db = getDB();
	$db->exec("INSERT INTO played_game_sequence(id) values(null);");
	return -intval($db->lastInsertRowID());
}

function decodeAndInsertGame($user,$game) {
	$badWords = Array();	
	$centerEid = getNodeEid($game['center']);
	$r1 = $game['relations'][0];
	$r2 = $game['relations'][1];

	if($centerEid === null)
		$badWords[] = $game['center'];
		
	foreach($game['cloud'] as $key => $w) {
		if ($w['name'] == "") continue;
		$cloudEid = getNodeEid($w['name']);
		$cloud[] = Array("eid" => $cloudEid,
								"pos" => $key,
								"d" => 5,
								"probaR1" => $w['relations'][0] ? "1" : "0",
								"probaR2" => $w['relations'][1] ? "1" : "0",
								"probaR0" => $w['relations'][2] ? "1" : "0",
								"probaTrash" => $w['relations'][3] ? "1" : "0");
		
		if($cloudEid === null)
			$badWords[] = $w['name'];
	}			

	if(count($badWords) > 0) {
		echo JSON_encode($badWords);
	} else if (count($cloud) < 5) {
		echo JSON_encode(false);
	} else {
		insertCreatedGame($centerEid,$cloud,$r1,$r2,10,$user);
		deleteOneGameCreation($user);
		echo JSON_encode(true);
	}
}

function insertCreatedGame($centerEid, $cloud, $r1, $r2, $totalDifficulty, $userName)
{
	$db = getDB();
	$sqlUserName = SQLite3::escapeString($userName);
	// Insère dans la base une partie avec le mot central $centerEid, le nuage $cloud et les relations $r1 et $r2
	$db->exec("begin transaction;");
	$db->exec("INSERT INTO game(gid, eid_central_word, relation_1, relation_2, difficulty, author, nb_like, nb_dislike)
		   VALUES (null, $centerEid, $r1, $r2, $totalDifficulty, '".$sqlUserName."', 0, 0);");
	$gid = $db->lastInsertRowID();
	
	$t = time();
	$pgid1 = getSequencePlayedGame();
	$db->exec("INSERT INTO played_game(pgid, gid, login, timestamp, like)"
			  ." VALUES ($pgid1, $gid, '".$sqlUserName."', $t, 0);");
	$pgid2 = getSequencePlayedGame();
	$db->exec("INSERT INTO played_game(pgid, gid, login, timestamp, like)"
			  ." VALUES ($pgid2, $gid, '".$sqlUserName."', $t, 0);");
	$pgid0 = getSequencePlayedGame();
	$db->exec("INSERT INTO played_game(pgid, gid, login, timestamp, like)"
			  ." VALUES ($pgid0, $gid, '".$sqlUserName."', $t, 0);");
	$pgidT = getSequencePlayedGame();
	$db->exec("INSERT INTO played_game(pgid, gid, login, timestamp, like)"
			  ." VALUES ($pgidT, $gid, '".$sqlUserName."', $t, 0);");

	// TODO : R0 et Trash + corrections
	foreach ($cloud as $c)
	{
		$totalWeight = $c['probaR1'] + $c['probaR2'] + $c['probaR0'] + $c['probaTrash'];
		$db->exec("INSERT INTO game_cloud(gid, num, difficulty, eid_word, totalWeight, probaR1, probaR2, probaR0, probaTrash)
			   VALUES ($gid, ".$c['pos'].", ".$c['d'].", ".$c['eid'].", $totalWeight, ".$c['probaR1'].", ".$c['probaR2'].", ".$c['probaR0'].", ".$c['probaTrash'].");");
		
		$db->exec("INSERT INTO played_game_cloud(pgid, login, gid, type, num, relation, weight, score)
			   VALUES ($pgid1, '".$sqlUserName."', $gid, 0, ".$c['pos'].", $r1, ".$c['probaR1'].", 0);");

		$db->exec("INSERT INTO played_game_cloud(pgid, login, gid, type, num, relation, weight, score)
			   VALUES ($pgid2, '".$sqlUserName."', $gid, 0, ".$c['pos'].", $r2, ".$c['probaR2'].", 0);");

		$db->exec("INSERT INTO played_game_cloud(pgid, login, gid, type, num, relation, weight, score)
			   VALUES ($pgid0, '".$sqlUserName."', $gid, 0, ".$c['pos'].", 0, ".$c['probaR0'].", 0);");

		$db->exec("INSERT INTO played_game_cloud(pgid, login, gid, type, num, relation, weight, score)
			   VALUES ($pgidT, '".$sqlUserName."', $gid, 0, ".$c['pos'].", -1, ".$c['probaTrash'].", 0);");
	}

	$db->exec("commit;");
}

/** Retourne un identifiant de partie aléatoire de la liste de parties jouables
* @return gid : Identifiant de partie.
*/
function randomGameCore() {
	$db = getDB();
	
	$q = $db->query('SELECT gid,(nb_like - nb_dislike + 5 * (author != "bot") + 5) AS coef FROM game');

	$game = array();
	$sum = 0;
	while($g = $q->fetchArray()) {
		if($g['coef'] > 0) {
			$game[] = $g;
			$sum += $g['coef'];
		}
	}
	
	$randomValue = rand(0,$sum-1);
	$count = 0;
	foreach($game as $g) {
		$count += $g['coef'];
		
		if($count >= $randomValue)
			return $g['gid'];
	}
	
	return null;

	// Ancienne requêtes de sélection.
	//return $db->querySingle("select gid from game where gid = (abs(random()) % (select max(gid) from game))+1 or gid = (select max(gid) from game where gid > 0) order by gid limit 1;");
}

/** Sélection aléatoire d'une partie de la base de données parmis les parties à jouer.
* @return gid : Identifiant de la partie selectionnée.
*/
function randomGame()
{
	$gid = randomGameCore();

	if ($gid === null) {
		// TODO : séparer ces créations de parties dans une fonction qui détecte le mode toussa.
		for ($i = 0; $i < 100; $i++)
			createGameCore(rand(5,15));

		$gid = randomGameCore();

		if ($gid === null)
			throw new Exception("Erreur lors de la récupération de la partie. Vérifiez qu'il y a au moins une partie.", 6);
	}
	return $gid;
}

/** Formatage des mots lorsqu'il y a des généralisations/spécifications par le symbole ">".
* @param word : Le mot que l'on veut reformater.
* @return word : le mot formaté.
*/
function formatWord($word) {
	$db = getDB();
	$res = "";
	$stack = array();

	while (($pos = strpos($word, ">")) !== false) {
		$res .= substr($word,0,$pos) . " (";
		$eid = intval(substr($word,$pos+1));
		if ($eid == 0) { throw new Exception("Erreur lors du suivi des pointeurs de spécialisation du mot $word.", 7); }
		if (in_array($eid, $stack)) { throw new Exception("Boucle rencontrée lors du suivi des pointeurs de spécialisation du mot $word.", 8); }
		if (count($stack) > 10) { throw new Exception("Trop de niveaux de récursions lors du suivi des pointeurs de spécialisation du mot $word.", 9); }
		$stack[] = $eid;
		$word = $db->querySingle("select name from node where eid = $eid");
	}

	$res .= $word;

	for ($depth = count($stack); $depth > 0; $depth--)
		$res .= ')';

	return $res;
}

function makePlayedGameRow($user, $gid, $pgid) {
	// TODO : planter si la requête suivante échoue pour quelque raison que ce soit.
	getDB()->exec("INSERT INTO played_game(pgid, gid, login, timestamp, like) VALUES (".longStrVal($pgid).", ".intval($gid).", '".SQLite3::escapeString($user)."', -1, 0);");
}

function getGidFromPgid($user, $pgid) {
	return getDB()->querySingle("SELECT gid FROM played_game WHERE pgid = ".longStrVal($pgid)." and login = '".SQLite3::escapeString($user)."';");
}

function makePgid($user, $gid = null, $pgid = null) {
	if ($gid === null && $pgid === null) {
		$gid = randomGame();
		$pgid = getSequencePlayedGame();
		makePlayedGameRow($user, $gid, $pgid);
	} elseif ($gid === null) {
		// On essaie de voir si ce pgid est déjà connu.
		$gid = getGidFromPgid($user, $pgid);
		if ($gid === null) {
			$gid = randomGame();
			makePlayedGameRow($user, $gid, $pgid);
		}
	} elseif ($pgid === null) {
		$pgid = getSequencePlayedGame();
		makePlayedGameRow($user, $gid, $pgid);
	} else {
		if ($gid != getGidFromPgid($user, $pgid)) throw new Exception("Cette partie n'est pas associée à votre nom d'utilisateur.", 4);
	}
	return array($gid, $pgid);
}

/** Formate une partie en JSON en l'imprimant.
* @param user : l'utilisateur.
* @param gameId : L'identifiant d'une partie.
*/
function game2array($user, $pgid = null)
{
	global $stringRelations;
	$db = getDB();

	list($gid, $pgid) = makePgid($user, null, $pgid);
	
	// TODO Yoann : faire des tests d'erreur pour ces select ?
	$game = $db->query("select author, gid, (select name from node where eid = eid_central_word) as name_central_word, eid_central_word, relation_1, relation_2 from game where gid = ".$gid.";");
	$game = $game->fetchArray();

	$ret = array(
		"author" => $game['author'],
		"gid" => $gid,
		"pgid" => $pgid,
		"relations" => array(
			array("id" => $game['relation_1'], "name" => ''.$stringRelations[$game['relation_1']]),
			array("id" => $game['relation_2'], "name" => ''.$stringRelations[$game['relation_2']]),
			array("id" => 0,                   "name" => ''.$stringRelations[0]),
			array("id" => -1,                  "name" => ''.$stringRelations[-1])
		),
		"center" => array("id" => $game['eid_central_word'], "name" => formatWord($game['name_central_word'])),
		"cloud" => array()
	);

	$res = $db->query("select eid_word,(select name from node where eid=eid_word) as name_word from game_cloud where gid = ".$gid.";");
	while ($x = $res->fetchArray())
	{
		$ret['cloud'][] = array("id" => $x['eid_word'], "name" => ''.formatWord($x['name_word']));
	}
	return $ret;
}

/** Création d'un lot de parties suivant un mode donnée.
* @param nbParties : le nombre de parties à créer.
* @param mode : Le mode de jeu pour les parties à créer.
*/
function createGame($nbParties, $mode)
{
	for ($i = 0; $i < $nbParties; $i++)
		createGameCore(rand(5,15));
}

/** Génère une partie (mode normal pour l'instant) pour une certaine taille de nuage.
* @param cloudSize : Taille du nuage.
*
* Est appelée par randomGame(), donc il faudra adapter quand on aura plusieurs modes, par exemple en ayant une fonction intermédiaire qui puisse être appelée par createGame et randomGame.
*/
function createGameCore($cloudSize)
{
	// select random node
	$centerEid = randomCenterNode();

	$r1 = cgChooseRelations(); $r2 = $r1[1]; $r1 = $r1[0];
	$sources = cgBuildResultSets($cloudSize, $centerEid, $r1, $r2); $sumWeights = $sources[1]; $sources = $sources[0];
	$cloud = cgBuildCloud($centerEid, $cloudSize, $sources, $sumWeights); $totalDifficulty = $cloud[1]; $cloud = $cloud[0];
	insertCreatedGame($centerEid, $cloud, $r1, $r2, $totalDifficulty, 'bot');
}

function computeScore($probas, $difficulty, $answer, $userReputation) {
	// Calcul du score. Score = proba[réponse de l'utilisateur]*coeff1 - proba[autres reponses]*coeff2 + bonus
	// score = - proba[autres reponses]*coeff2
	// On aura donc -5 <= score <= 0
	$score = -5 * (($probas[0] + $probas[1] + $probas[2] + $probas[3]) - $probas[$answer]);
	
	// score = proba[réponse de l'utilisateur]*coeff1 - proba[autres reponses]*coeff2
	// On aura donc -5 <= score <= 10
	$score += 10 * $probas[$answer];
	
	// On est indulgent si la réponse est 3 (poubelle) :
	if ($answer == 3 && $score < 0) {
		$score = $score / 2;
	}
	
	// Adapter le score en fonction de la réputation de l'utilisateur (quand il est jeune, augmenter le score pour le motiver).
	// On aura donc -5 <= score <= 15
	if ($score > 3) {
		$score += max(0, min(5, 5 - $userReputation));
	}
	
	return round($score);
}

/** Calcul de la réputation de l'utilisateur selon son score.
* @param score : Le score du joueur.
*/
function computeUserReputation($score) {
	return max(round(log(max($score/10,1))*100)/100, 0);
}

/** Formatage des probalitées dans un tableau.
* @param row : le vecteur de probabilités.
* @return array : Le vecteur de probabilités normalisé.
*/
function normalizeProbas($row) {
	return array($row['probaR1']/$row['totalWeight'], $row['probaR2']/$row['totalWeight'], $row['probaR0']/$row['totalWeight'], $row['probaTrash']/$row['totalWeight']);
}

/** Insertion des données d'une partie joué par un utilisateur.
* @param user : L'identifiant de l'utilisateur ayant joué à la partie.
* @param pgid : L'identifiant de la partie jouée.
* @param gid : L'identifiant du jeu auquel la partie appartient.
* @return score : Le score réalisé par le joueur.
*/
function setGame($user, $pgid, $gid, $answers)
{
	$db = getDB();
	
	if ('ok' !== $db->querySingle("SELECT 'ok' FROM played_game WHERE pgid = ".longStrVal($pgid)." and gid = ".intval($gid)." and login = '".SQLite3::escapeString($user)."' and timestamp = -1;")) {
		return getGameScores($user, $pgid, $gid);
	}
	
	$userReputation = computeUserReputation($db->querySingle("SELECT score FROM user WHERE login = '".SQLite3::escapeString($user)."';"));
	
	$db->exec("begin transaction;");
	$db->exec("update played_game set timestamp = ".time()." where pgid = ".longStrVal($pgid)." and login = '".SQLite3::escapeString($user)."';");

	$r0 = 0;
	$trash = -1;
	$r1 = $db->querySingle("SELECT relation_1, relation_2 FROM game WHERE gid = ".intval($gid).";", true);
	$r2 = $r1['relation_2'];
	$r1 = $r1['relation_1'];
	$res = $db->query("SELECT num, difficulty, totalWeight, probaR1, probaR2, probaR0, probaTrash FROM game_cloud WHERE gid = ".intval($gid).";");
	$gameScore = 0;
	$scores = array();
	
	while ($row = $res->fetchArray())
	{
		$num = intval($row['num']);
		if (!isset($answers[$num])) {
			throw new Exception("Cette requête \"Set partie\" ne donne pas de réponse (une relation) pour le mot numéro ".($num+1)." de la partie.", 5);
		}
		$relanswer = intval($answers[$num]);

		switch ($relanswer) 
		{
			case $r1:    $answer = 0; $probaRx = "probaR1"; break;
			case $r2:    $answer = 1; $probaRx = "probaR2"; break;
			case $r0:    $answer = 2; $probaRx = "probaR0"; break;
			case $trash: $answer = 3; $probaRx = "probaTrash"; break;
			default:     throw new Exception("Réponse ($relanswer) invalide pour le mot $num. Les réponses possibles sont : $r1, $r2, $r0, $trash", 5);
		}
			
		$wordScore = computeScore(normalizeProbas($row), $row['difficulty'], $answer, $userReputation);
		$gameScore += $wordScore;
		$scores[$num] = $wordScore;
		
		$db->exec("insert into played_game_cloud(pgid, login, gid, type, num, relation, weight, score)"
				  ." values(".longStrVal($pgid).", '".SQLite3::escapeString($user)."', ".intval($gid).", 1, $num, $r1, ".$userReputation.", ".$wordScore.");");
		$db->exec("update game_cloud set $probaRx = $probaRx + ".max(min($userReputation/2,5),0)." where gid = ".intval($gid)." and num = $num;");
		$db->exec("update game_cloud set totalWeight = totalWeight + ".max(min($userReputation/2,5),0)." where gid = ".intval($gid)." and num = $num;");
	}
	$db->exec("update user set score = score + ".$gameScore." where login = '$user';");

	$db->exec("commit;");
	
	return array(
		'scoreTotal' => $gameScore,
		'alreadyPlayed' => false,
		'scores' => $scores
	);
}

function getGameScores($user, $pgid, $gid) {
	$db = getDB();
	$timestamp = $db->querySingle("SELECT timestamp FROM played_game WHERE pgid = ".longStrVal($pgid)." and $gid = ".intval($gid)." and login = '".SQLite3::escapeString($user)."';");
	if ($timestamp == -1) {
		throw new Exception("Cette partie n'a jamais été jouée.", 4); // TODO : code d'erreur en doublon avec celui ci-dessous.
	} elseif ($timestamp == null) {
		throw new Exception("Cette partie n'est pas associée à votre nom d'utilisateur.", 4);
	}
	
	$gameScore = 0;
	$scores = array();
	$res = $db->query("SELECT num, score from played_game_cloud where pgid = ".longStrVal($pgid)." and gid = ".intval($gid)." and login = '".SQLite3::escapeString($user)."';");
	while ($row = $res->fetchArray())
	{
		$gameScore += $row['score'];
		$scores[$row['num']] = $row['score'];
	}
	return array(
		'scoreTotal' => $gameScore,
		'alreadyPlayed' => true,
		'scores' => $scores
	);
}

/** Fourni l'ensembles des relations pouvant apparaître dans le jeu.
* @return JSON type et nom des relations.
*/
function getGameRelations() {
	$json = "{";
	global $stringRelations;
	
	foreach($stringRelations as $id=>$description)
		if($id == -1)
			$json .= '"'.$id.'":"'.$description.'"';
		else
			$json .= ',"'.$id.'":"'.$description.'"';
			
	$json .= '}';
	
	return $json;
}

function setGameGetScore($user, $pgid, $answers) {
	$scores = setGame($user, $pgid, getGidFromPgid($user, $pgid), $answers);
	$g = game2array($user, $pgid);
	$s = array();
	foreach ($g['cloud'] as $k => $v) {
		$s[] = array('name' => $v['name'], 'score' => $scores['scores'][$k]);
	}
	$scores['scores'] = $s;
	$scores['author'] = $g['author'];
	// On renvoie une nouvelle partie pour garder le client toujours bien alimenté.
	$scores['newGame'] = game2array($user);
	$scores['minScore'] = -5;
	$scores['maxScore'] = 10;
	echo JSON_encode($scores);
}


/** retourne l'eid d'un mot.
* @param node : le mot dont on veut obtenir l'eid.
*/
function getNodeEid($node) {
	$db = getDB();

	return $db->querySingle("SELECT eid FROM node WHERE name='".SQLite3::escapeString($node)."';");
}

/* Permet de déterminer si le mot existe dans la base de données ou non.
 * @param node : Le mot dont on veut connaître l'existance.
 * @return char : "0" ou "1".
*/
function wordExist($node) {
	$db = getDB();

	return getNodeEid($node) === null ? "0" : "1";
}

function getUserInfo($user, $key) {
	return getdb()->querySingle("SELECT value FROM user_info WHERE user = '".sqlite3::escapestring($user)."' AND key = '".SQLite3::escapeString($key)."';");
}

function setUserInfo($user, $key, $value) {
	getDB()->exec("INSERT OR REPLACE INTO user_info(user, key, value) values('".SQLite3::escapeString($user)
				  ."', '".SQLite3::escapeString($key)
				  ."', '".SQLite3::escapeString($value)."');");
}

function userPrefsDefaults() {
	return Array(
		"theme" => "green"
	);
}

function userPrefs($user) {
	$res = userPrefsDefaults();
	foreach ($res as $k => &$v) {
		$x = getUserInfo($user, $k);
		if ($x !== null) $v = $x;
	}
	$res['connected'] = true;
	echo JSON_encode($res);
}

function setUserPref($user, $key, $value) {
	if (array_key_exists($key, userPrefsDefaults())) {
		setUserInfo($user, $key, $value);
	}
}

function getJAimePgid($user, $pgid) {
	getDB()->querySingle("SELECT like FROM played_game WHERE login = '".SQLite3::escapeString($user)."' and pgid = ".longStrVal($pgid).";");
}

function setJAimePgid($user, $pgid, $value) {
	$value = intval($value) / abs(intval($value) || 1); // Calcule le signe de $value : -1 ou 0 ou 1
	$original = getJAimePgid($user, $pgid);
	getDB()->querySingle("UPDATE played_game SET like = ".$value." WHERE login = '".SQLite3::escapeString($user)."' and pgid = ".longStrVal($pgid).";");
	$gid = getGidFromPgid($user, longStrVal($pgid));
	getDB()->querySingle("UPDATE game SET nb_like = nb_like".($original == 1 ? " - 1" : "").($value == 1 ? " + 1" : "")." WHERE gid = ".intval($gid).";");
	getDB()->querySingle("UPDATE game SET nb_dislike = nb_dislike".($original == -1 ? " - 1" : "").($value == -1 ? " + 1" : "")." WHERE gid = ".intval($gid).";");
}

function addGameCreationBonus($user, $nb) {
	if($nb > 0) {
		getDB()->exec('UPDATE user SET cgCount = cgCount + '.$ng.' WHERE login=\''.$user.'\'');
		return true;
	}

	return false;
}

function deleteOneGameCreation($user) {
	if(getNbGameCreationRemained($user) > 0)
		getDB()->exec('UPDATE user SET cgCount=cgCount-1 WHERE login=\''.$user.'\'');
}

function getNbGameCreationRemained($user) {
	return getDB()->querySingle('SELECT cgCount FROM user WHERE login=\''.$user.'\'');
}

?>
