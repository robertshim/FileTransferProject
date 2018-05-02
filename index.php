<html>

<head>
<title>File Upload</title>
</head>

<body>
<h1>FILE UPLOAD</h1>

<form method='post' action='fileupload.php' enctype='multipart/form-data'>

Select File : <input type='file' name='filename[]' id='filename' size='10' multiple="multiple"/>
<input type='submit' value='fileupload'>
</form>


<h1>FILE DOWNLOAD</h1>
<!--form method='post' action='filedownload.php' enctype='multipart/form-data' name='filedown'-->
<table border="1">




<?php

$results=array();
$dir='./uploads/';
$open_d=opendir($dir);
$i=0;
//echo "<input type=\"checkbox\" name=\"chkAll\" onClick=\"javascript:checkAll();\"/>All<br/>";
while($file = readdir($open_d)){
	$results[$i]=$file;

	echo "<tr><td><a href='http://localhost/uploads/".$results[$i]."' download>";

	echo $results[$i];	
	echo "</a></td></tr>";
	//echo "</td></tr>";
	$i++;
}

echo $file[5];

closedir($open_d);


?>
<input type='submit' value='filedownload' onClick="document.filedown.submit();">
</table>

</form>

</body>

</html>
