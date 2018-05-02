<?php


$cnt=count($_FILES['filename']['name']);
echo "<br/>".$cnt."<br/><br/>";

$uploaddir = './uploads/';
for($i=0; $i<$cnt; $i++){
	$uploadfile[$i] = $uploaddir.basename($_FILES['filename']['name'][$i]);
}


echo $_FILES;
$i=0;

while($i<$cnt){
	echo "<tr>";
	if(move_uploaded_file($_FILES['filename']['tmp_name'][$i], $uploadfile[$i])){

		
	}
	else
	{
		echo "<br/>Fail<br/>";
	}
	echo "</tr>";
	$i++;
}
echo "<script>location.href='index.php'</script>";

echo "</body></html>";

?>







