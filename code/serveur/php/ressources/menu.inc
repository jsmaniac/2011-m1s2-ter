		<div class="menu">
			<h1>PtiClic <span class="android">sous Android™</span> - Version 0.1 Alpha</h1>
			
			<span id="links">
				<a href="index.php">Accueil</a>
				<a href="download.php">Téléchargement</a>
				<!-- <a href=".php">Créer des parties</a> -->
			        <a href="contact.php">Contact</a>
			<?php
			if(!isset($_SESSION['userId']))				// Menu hors connexion.
			{?>
			        <a id="cnx" href="login.php">Se connecter</a>
				<a id="cnx" href="signup.php">S'incrire</a>
			
			<?php }
			else							// Menu une fois connecté.
			{?>
				<a id="cnx" href="login.php?d=true">Se deconnecter</a>
			<?php } ?>
			</span>
		</div>